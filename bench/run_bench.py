#!/usr/bin/env python3
"""캐시 사용 전후 성능 비교 (협약 15.4).

같은 샘플 API를 캐시 OFF(?cache=off)와 캐시 ON으로 호출한다. 시나리오마다 두 서버를 새로 띄워
같은 초기 상태(빈 캐시, 원본 조회 0회)에서 같은 요청 순서·같은 동시 요청 수로 측정한다.
첫 요청(캐시 빈 상태)은 혼자 보내 따로 재고, 나머지는 동시에 보낸다.

실행 : python3 bench/run_bench.py build/mini_redis build/sample_api
결과 : 화면에 표, bench/results/bench_result.json에 원본 수치
"""

import concurrent.futures
import http.client
import json
import os
import platform
import random
import socket
import statistics
import subprocess
import sys
import time

REDIS_BIN = sys.argv[1] if len(sys.argv) > 1 else "build/mini_redis"
DEMO_BIN = sys.argv[2] if len(sys.argv) > 2 else "build/sample_api"
REDIS_PORT = 16392
DEMO_PORT = 13092
HOST = "127.0.0.1"
REQUEST_COUNT = 200
CONCURRENCY = 10
KEY_SPACE = 100
RANDOM_SEED = 42
SWEEP_REQUEST_COUNT = 100
SWEEP_CONCURRENCIES = [1, 10, 50]
RESULT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "results",
                           "bench_result.json")


def make_scenarios():
    random_ids = random.Random(RANDOM_SEED)
    return [
        ("1. 같은 상품 1개 반복", [1] * REQUEST_COUNT),
        ("2. 상품 100개 무작위", [random_ids.randint(1, KEY_SPACE) for _ in range(REQUEST_COUNT)]),
        ("3. 모든 요청이 새 상품", list(range(1, REQUEST_COUNT + 1))),
    ]


def wait_for_port(port):
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        try:
            with socket.create_connection((HOST, port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError(f"포트 {port}의 서버가 뜨지 않았습니다")


class Servers:
    """with 블록 동안 미니 레디스와 샘플 API를 새로 띄운다 (같은 초기 상태)."""

    def __enter__(self):
        self.redis = subprocess.Popen([REDIS_BIN, str(REDIS_PORT)], stdout=subprocess.DEVNULL)
        wait_for_port(REDIS_PORT)
        self.demo = subprocess.Popen([DEMO_BIN, str(DEMO_PORT), str(REDIS_PORT)],
                                     stdout=subprocess.DEVNULL)
        wait_for_port(DEMO_PORT)
        return self

    def __exit__(self, *exc_info):
        for process in (self.demo, self.redis):
            process.terminate()
            process.wait(timeout=5)


def get(path):
    """(지연 시간 ms, 상태 코드, X-Cache 헤더)"""
    started = time.perf_counter()
    connection = http.client.HTTPConnection(HOST, DEMO_PORT, timeout=30)
    try:
        connection.request("GET", path)
        response = connection.getresponse()
        response.read()
        return (time.perf_counter() - started) * 1000, response.status, response.getheader("X-Cache")
    finally:
        connection.close()


def origin_call_count():
    connection = http.client.HTTPConnection(HOST, DEMO_PORT, timeout=5)
    try:
        connection.request("GET", "/demo/stats")
        return json.loads(connection.getresponse().read())["originCallCount"]
    finally:
        connection.close()


def percentile(values, ratio):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(len(ordered) * ratio))]


def run_once(ids, use_cache, concurrency):
    """첫 요청은 혼자, 나머지는 concurrency개씩 동시에 보낸다."""
    query = "" if use_cache else "?cache=off"
    paths = [f"/products/{product_id}{query}" for product_id in ids]
    with Servers():
        first = get(paths[0])
        started = time.perf_counter()
        with concurrent.futures.ThreadPoolExecutor(concurrency) as executor:
            rest = list(executor.map(get, paths[1:]))
        elapsed = time.perf_counter() - started
        origin_calls = origin_call_count()
    latencies = [result[0] for result in rest]
    hit_count = sum(1 for result in [first] + rest if result[2] == "HIT")
    return {
        "cache": "ON" if use_cache else "OFF",
        "requests": len(paths),
        "concurrency": concurrency,
        "errors": sum(1 for result in [first] + rest if result[1] != 200),
        "first_ms": round(first[0], 2),
        "avg_ms": round(statistics.mean(latencies), 2),
        "p95_ms": round(percentile(latencies, 0.95), 2),
        "rps": round(len(rest) / elapsed, 1),
        "hit_rate": round(hit_count / len(paths), 3) if use_cache else None,
        "origin_calls": origin_calls,
    }


def machine_info():
    cpu = platform.processor() or "unknown"
    try:
        with open("/proc/cpuinfo", encoding="utf-8") as cpuinfo:
            cpu = next(line.split(":", 1)[1].strip() for line in cpuinfo if "model name" in line)
    except (OSError, StopIteration):
        pass
    return {"cpu": cpu, "logical_cores": os.cpu_count(), "os": platform.platform(),
            "python": platform.python_version()}


def print_table(title, rows):
    print(f"\n## {title}\n")
    print("| 시나리오 | 캐시 | 동시 | 첫 요청 | 평균 | p95 | 초당 요청 수 | 적중률 | 원본 조회 | 오류 |")
    print("|---|---|---|---|---|---|---|---|---|---|")
    for row in rows:
        hit = "-" if row["hit_rate"] is None else f"{row['hit_rate'] * 100:.1f}%"
        print(f"| {row['scenario']} | {row['cache']} | {row['concurrency']} | {row['first_ms']}ms | "
              f"{row['avg_ms']}ms | {row['p95_ms']}ms | {row['rps']} | {hit} | "
              f"{row['origin_calls']} | {row['errors']} |")


def main():
    scenario_rows = []
    for name, ids in make_scenarios():
        for use_cache in (False, True):
            row = run_once(ids, use_cache, CONCURRENCY)
            row["scenario"] = name
            scenario_rows.append(row)
            print(f"측정 완료 : {name} 캐시 {row['cache']}", file=sys.stderr)
    sweep_rows = []
    sweep_ids = make_scenarios()[1][1][:SWEEP_REQUEST_COUNT]
    for concurrency in SWEEP_CONCURRENCIES:
        for use_cache in (False, True):
            row = run_once(sweep_ids, use_cache, concurrency)
            row["scenario"] = f"2. 무작위 {SWEEP_REQUEST_COUNT}건"
            sweep_rows.append(row)
            print(f"측정 완료 : 동시 {concurrency} 캐시 {row['cache']}", file=sys.stderr)
    result = {"conditions": {"requests": REQUEST_COUNT, "concurrency": CONCURRENCY,
                             "key_space": KEY_SPACE, "random_seed": RANDOM_SEED,
                             "cache_ttl_ms": 60000, "slow_source_delay_ms": 300,
                             "keep_alive": False, "machine": machine_info()},
              "scenarios": scenario_rows, "concurrency_sweep": sweep_rows}
    os.makedirs(os.path.dirname(RESULT_PATH), exist_ok=True)
    with open(RESULT_PATH, "w", encoding="utf-8") as result_file:
        json.dump(result, result_file, ensure_ascii=False, indent=2)
    print_table(f"시나리오별 (요청 {REQUEST_COUNT}개, 동시 {CONCURRENCY})", scenario_rows)
    print_table("동시 요청 수에 따른 변화", sweep_rows)
    print(f"\n측정 환경 : {json.dumps(result['conditions']['machine'], ensure_ascii=False)}")


if __name__ == "__main__":
    main()
