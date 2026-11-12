#!/usr/bin/env python3
"""같은 C API, 원본, 순서, 동시성, 빈 초기 상태의 캐시 ON/OFF 실측."""
import argparse
import concurrent.futures
import datetime
import http.client
import json
import math
import os
import pathlib
import platform
import random
import socket
import statistics
import subprocess
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]


def call(port, method, path):
    connection = http.client.HTTPConnection('127.0.0.1', port, timeout=10)
    try:
        start = time.perf_counter()
        connection.request(method, path)
        response = connection.getresponse()
        body = json.loads(response.read())
        elapsed_ms = (time.perf_counter() - start) * 1000
        if response.status != 200:
            raise RuntimeError((response.status, body))
        return elapsed_ms, response.getheader('X-Cache'), body
    finally:
        connection.close()


def free_port():
    with socket.socket() as connection:
        connection.bind(('127.0.0.1', 0))
        return connection.getsockname()[1]


def ready(process, port, path):
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError('server exited early')
        try:
            call(port, 'GET', path)
            return
        except OSError:
            time.sleep(.02)
    raise RuntimeError('readiness timeout')


def stop(process):
    if process.poll() is None:
        process.terminate()
        process.wait(timeout=5)
    if process.returncode != 0:
        raise RuntimeError(f'server exited {process.returncode}')


def measure(args, scenario, sequence, mode, round_number):
    redis_port, demo_port = free_port(), free_port()
    while demo_port == redis_port:
        demo_port = free_port()
    redis = subprocess.Popen([str(ROOT / 'build/mini_redis'), str(redis_port)],
                             stdout=subprocess.DEVNULL)
    demo = None
    try:
        ready(redis, redis_port, '/stats')
        demo = subprocess.Popen([str(ROOT / 'build/sample_api'), str(demo_port), str(redis_port),
                                 str(args.ttl_ms), str(args.delay_ms)], stdout=subprocess.DEVNULL)
        ready(demo, demo_port, '/demo/stats')
        assert call(demo_port, 'GET', '/demo/stats')[2]['originCallCount'] == 0
        assert call(redis_port, 'GET', '/stats')[2]['size'] == 0
        suffix = '?cache=off' if mode == 'OFF' else ''
        def get_product(product_id):
            result = call(demo_port, 'GET', '/products/' + str(product_id) + suffix)
            assert result[2] == {'value': str(product_id)}, result
            return result
        # 첫 요청은 별도로 기다린 뒤 나머지를 같은 concurrency로 보낸다. 예열 없음.
        start = time.perf_counter()
        first = get_product(sequence[0])
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.concurrency) as pool:
            samples = [first] + list(pool.map(get_product, sequence[1:]))
        wall_seconds = time.perf_counter() - start
        origin_count = call(demo_port, 'GET', '/demo/stats')[2]['originCallCount']
        store_stats = call(redis_port, 'GET', '/stats')[2]
        hits = sum(sample[1] == 'HIT' for sample in samples)
        assert origin_count == len(samples) - hits
        latencies = [sample[0] for sample in samples]
        return {
            'scenario': scenario, 'cache': mode, 'round': round_number,
            'request_sequence': sequence, 'latencies_ms': latencies,
            'mean_ms': statistics.mean(latencies),
            'p95_ms': sorted(latencies)[math.ceil(.95 * len(latencies)) - 1],
            'cold_ms': first[0], 'remaining_mean_ms': statistics.mean(latencies[1:]),
            'wall_seconds': wall_seconds, 'throughput_rps': len(samples) / wall_seconds,
            'hits': hits, 'hit_rate': hits / len(samples), 'origin_calls': origin_count,
            'store_stats': store_stats,
        }
    finally:
        if demo is not None:
            stop(demo)
        stop(redis)


def cpu_description():
    try:
        return subprocess.check_output(['sysctl', '-n', 'machdep.cpu.brand_string'],
                                       text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return platform.processor() or platform.machine()


def write_report(args, results, metadata):
    lines = ['# 캐시 ON/OFF 실측', '', '자동 생성: `python3 bench/compare.py`.', '',
             f"측정 시각: {metadata['timestamp']}. OS: {metadata['platform']}.",
             f"CPU: {metadata['cpu']}, 논리 CPU {metadata['cpu_count']}개.", '',
             f'- 각 행은 {args.requests}요청 × {args.rounds}회. 동시 요청 수 {args.concurrency}.',
             f'- 원본 지연 {args.delay_ms}ms, TTL {args.ttl_ms}ms.',
             '- 매 시나리오·모드·반복마다 두 프로세스를 새로 시작해 원본 수정·횟수·캐시를 초기화.',
             '- 같은 상품 ID 순서. 무작위는 seed 42. ON/OFF 실행 순서를 반복마다 교대.',
             '- 첫 1회는 빈 캐시에서 단독 실행하고, 나머지는 고정 크기 스레드 풀로 호출.',
             '- HTTP/1.1, 요청마다 새 연결, keep-alive 없음. 루프백 C 서버, Python 표준 라이브러리.',
             '- 지연시간은 클라이언트 연결·전송·응답 읽기 포함. 처리량은 전체 벽시계 기준.',
             '', '| 시나리오 | 캐시 | 평균 ms | p95 ms | 요청/초 | HIT % | 원본 조회/회 | 첫 요청 ms | 나머지 평균 ms |',
             '|---|---|---:|---:|---:|---:|---:|---:|---:|']
    for scenario in ['repeat', 'random100', 'unique']:
        for mode in ['OFF', 'ON']:
            group = [row for row in results if row['scenario'] == scenario and row['cache'] == mode]
            latencies = [value for row in group for value in row['latencies_ms']]
            count = len(latencies)
            p95 = sorted(latencies)[math.ceil(.95 * count) - 1]
            lines.append(f'| {scenario} | {mode} | {statistics.mean(latencies):.3f} | {p95:.3f} | '
                         f'{count / sum(row["wall_seconds"] for row in group):.2f} | '
                         f'{100 * sum(row["hits"] for row in group) / count:.2f} | '
                         f'{statistics.mean(row["origin_calls"] for row in group):.2f} | '
                         f'{statistics.mean(row["cold_ms"] for row in group):.3f} | '
                         f'{statistics.mean(row["remaining_mean_ms"] for row in group):.3f} |')
    lines += ['', 'repeat는 같은 상품 42, random100은 상품 100개에서 고정 난수 추출, unique는 '
              '요청마다 새 상품이다. 원본 횟수는 한 반복당 평균이며 원본 원장으로 확인한다.', '',
              '반복 조회는 최초 원본 접근 이후 HTTP 캐시 조회만 수행한다. 새 키만 조회하면 적중이 '
              '없어 캐시 GET·PUT 네트워크 비용이 더해진다. 짧은 루프백 측정의 작은 차이는 '
              '스케줄링과 측정 변동을 포함하며 성능 향상을 보장하지 않는다. 원본 지연은 '
              '대기이므로 동시성에 따라 처리량이 달라진다. 동일 MISS의 몰림 방지는 구현하지 않았다.', '',
              '매 요청 지연, 요청 순서, 반복별 통계와 환경은 [results.json](../bench/results.json)에 '
              '보존한다. 스크립트는 HTTP만 사용하며 저장소를 직접 링크하지 않는다.', '',
              '재현: `make && python3 bench/compare.py --requests '
              f'{args.requests} --concurrency {args.concurrency} --rounds {args.rounds} '
              f'--delay-ms {args.delay_ms} --ttl-ms {args.ttl_ms}`.', '',
              '100만 회 엔진 마이크로벤치마크는 권장 선택 항목이며 이번 HTTP 캐시 비교에는 포함하지 않았다.']
    (ROOT / 'docs/benchmark.md').write_text('\n'.join(lines) + '\n')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--requests', type=int, default=60)
    parser.add_argument('--concurrency', type=int, default=4)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--delay-ms', type=int, default=300)
    parser.add_argument('--ttl-ms', type=int, default=60000)
    args = parser.parse_args()
    if args.requests < 2 or not 1 <= args.concurrency <= 32 or args.rounds < 1:
        parser.error('requests >= 2, 1 <= concurrency <= 32, rounds >= 1')
    if not 0 <= args.delay_ms <= 2147483647 or not 1 <= args.ttl_ms <= 2147483647:
        parser.error('delay-ms/ttl-ms range')
    generator = random.Random(42)
    sequences = {'repeat': [42] * args.requests,
                 'random100': [generator.randrange(100) for _ in range(args.requests)],
                 'unique': list(range(args.requests))}
    results = []
    for scenario, sequence in sequences.items():
        for round_number in range(args.rounds):
            modes = ['OFF', 'ON'] if round_number % 2 == 0 else ['ON', 'OFF']
            for mode in modes:
                row = measure(args, scenario, sequence, mode, round_number + 1)
                results.append(row)
                print(f'{scenario} {mode} #{round_number + 1}: {row["mean_ms"]:.3f}ms, '
                      f'{row["throughput_rps"]:.2f}req/s, hit={row["hit_rate"]:.1%}, '
                      f'origin={row["origin_calls"]}', flush=True)
    metadata = {'timestamp': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                'platform': platform.platform(), 'cpu': cpu_description(),
                'cpu_count': os.cpu_count(), 'conditions': vars(args)}
    (ROOT / 'bench/results.json').write_text(json.dumps({'metadata': metadata, 'runs': results},
                                                      ensure_ascii=False, indent=2) + '\n')
    write_report(args, results, metadata)


if __name__ == '__main__':
    main()
