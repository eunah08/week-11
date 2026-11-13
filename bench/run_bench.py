"""협약 15.4 성능 비교. 같은 샘플 API를 캐시 OFF(?cache=off)와 ON으로 호출해 비교한다.
미니 레디스와 샘플 API를 먼저 띄운다. 미니 레디스는 HTTP API로만 쓴다. (협약 6장)
실행 : python3 bench/run_bench.py [요청 수] [동시 요청 수]
결과 : 표를 출력하고 원본 결과를 bench/result.json에 저장한다.
"""

import http.client
import json
import os
import random
import sys
import time
from concurrent.futures import ThreadPoolExecutor

SAMPLE_API_ADDRESS = ("127.0.0.1", 3000)
MINI_REDIS_ADDRESS = ("127.0.0.1", 6380)
DEFAULT_REQUEST_COUNT = 200
DEFAULT_CONCURRENCY = 10
PRODUCT_KIND_COUNT = 100
RANDOM_SEED = 42                 # 같은 요청 순서를 쓰려고 고정한다
NEW_PRODUCT_ID_START = 1000000
P95_RATIO = 0.95
REQUEST_TIMEOUT_SECONDS = 30
RESULT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "result.json")


def send_get(address, path):
    """(상태 코드, 응답 본문, X-Cache 헤더)"""
    connection = http.client.HTTPConnection(*address, timeout=REQUEST_TIMEOUT_SECONDS)
    connection.request("GET", path)
    response = connection.getresponse()
    body = response.read()
    x_cache = response.getheader("X-Cache")
    connection.close()
    return response.status, body, x_cache


def reset_cache():
    """같은 초기 상태(빈 캐시)에서 시작하려고 product: 키를 모두 무효화한다"""
    connection = http.client.HTTPConnection(*MINI_REDIS_ADDRESS, timeout=REQUEST_TIMEOUT_SECONDS)
    connection.request("POST", "/invalidate", body=b'{"prefix": "product:"}')
    connection.getresponse().read()
    connection.close()


def origin_call_count():
    return json.loads(send_get(SAMPLE_API_ADDRESS, "/demo/stats")[1])["originCallCount"]


def make_product_ids(scenario, request_count):
    if scenario == 1:
        return [1] * request_count
    if scenario == 2:
        generator = random.Random(RANDOM_SEED)
        return [generator.randint(1, PRODUCT_KIND_COUNT) for _ in range(request_count)]
    return [NEW_PRODUCT_ID_START + i for i in range(request_count)]


def timed_get(path):
    start = time.perf_counter()
    status, _, x_cache = send_get(SAMPLE_API_ADDRESS, path)
    return time.perf_counter() - start, status, x_cache


def run_case(scenario, use_cache, request_count, concurrency):
    reset_cache()
    query = "" if use_cache else "?cache=off"
    paths = [f"/products/{product_id}{query}" for product_id in make_product_ids(scenario,
                                                                                request_count)]
    calls_before = origin_call_count()
    start = time.perf_counter()
    with ThreadPoolExecutor(concurrency) as pool:
        results = list(pool.map(timed_get, paths))
    elapsed_seconds = time.perf_counter() - start

    latencies = sorted(result[0] for result in results)
    hit_count = sum(1 for result in results if result[2] == "HIT")
    return {
        "scenario": scenario,
        "cache": "ON" if use_cache else "OFF",
        "averageMs": sum(latencies) / len(latencies) * 1000,
        "p95Ms": latencies[int(len(latencies) * P95_RATIO) - 1] * 1000,
        "requestsPerSecond": request_count / elapsed_seconds,
        "hitRate": hit_count / request_count if use_cache else None,
        "originCallCount": origin_call_count() - calls_before,
        "errorCount": sum(1 for result in results if result[1] != 200),
    }


def print_row(result):
    hit_rate = "-" if result["hitRate"] is None else f"{result['hitRate'] * 100:.1f}%"
    print(f"| {result['scenario']} | {result['cache']} | {result['averageMs']:.1f}ms "
          f"| {result['p95Ms']:.1f}ms | {result['requestsPerSecond']:.1f} | {hit_rate} "
          f"| {result['originCallCount']} | {result['errorCount']} |")


def main():
    request_count = int(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_REQUEST_COUNT
    concurrency = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_CONCURRENCY
    print(f"요청 수 {request_count}, 동시 요청 수 {concurrency}, 상품 종류 {PRODUCT_KIND_COUNT}")
    print("| 시나리오 | 캐시 | 평균 | p95 | 초당 요청 수 | 적중률 | 원본 조회 | 실패 |")
    print("|---|---|---|---|---|---|---|---|")

    results = []
    for scenario in (1, 2, 3):
        for use_cache in (False, True):
            result = run_case(scenario, use_cache, request_count, concurrency)
            print_row(result)
            results.append(result)

    condition = {"requestCount": request_count, "concurrency": concurrency,
                 "productKindCount": PRODUCT_KIND_COUNT}
    with open(RESULT_PATH, "w", encoding="utf-8") as result_file:
        json.dump({"condition": condition, "results": results}, result_file, indent=2)


main()
