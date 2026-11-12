#!/usr/bin/env python3
"""실제 두 C 서버를 띄워 HTTP 캐시·원본 실패·캐시 종료를 검증한다."""
import concurrent.futures
import http.client
import json
import os
import pathlib
import socket
import subprocess
import time
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILD = ROOT / os.environ.get('MINI_REDIS_BUILD_DIR', 'build')


def free_port():
    with socket.socket() as connection:
        connection.bind(('127.0.0.1', 0))
        return connection.getsockname()[1]


def call(port, method, path, body=None):
    connection = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
    try:
        connection.request(method, path, None if body is None else json.dumps(body))
        response = connection.getresponse()
        return response.status, json.loads(response.read()), response.getheader('X-Cache')
    finally:
        connection.close()


def wait_ready(process, port, path):
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise AssertionError(f'server exited: {process.returncode}')
        try:
            if call(port, 'GET', path)[0] == 200:
                return
        except OSError:
            pass
        time.sleep(.02)
    raise AssertionError('server readiness timeout')


def stop(process):
    if process.poll() is None:
        process.terminate()
        process.wait(timeout=5)
    if process.returncode != 0:
        raise AssertionError(f'server exit: {process.returncode}')


class DemoTest(unittest.TestCase):
    def test_cache_lifecycle_and_outage(self):
        redis_port, demo_port = free_port(), free_port()
        redis = subprocess.Popen([str(BUILD / 'mini_redis'), str(redis_port)])
        demo = None
        try:
            wait_ready(redis, redis_port, '/stats')
            # port, redis_port, ttl_ms, origin_delay_ms
            demo = subprocess.Popen([str(BUILD / 'sample_api'), str(demo_port),
                                     str(redis_port), '500', '30'])
            wait_ready(demo, demo_port, '/demo/stats')
            first = call(demo_port, 'GET', '/products/42')
            self.assertEqual(first[0], 200)
            self.assertEqual(first[2], 'MISS')
            self.assertEqual(call(demo_port, 'GET', '/demo/stats')[1]['originCallCount'], 1)
            second = call(demo_port, 'GET', '/products/42')
            self.assertEqual(second[1], first[1])
            self.assertEqual(second[2], 'HIT')
            self.assertEqual(call(demo_port, 'GET', '/demo/stats')[1]['originCallCount'], 1)
            self.assertEqual(call(demo_port, 'PUT', '/products/42', {'value': 'new'})[0], 200)
            self.assertEqual(call(redis_port, 'GET', '/keys/product:42')[0], 404)
            updated = call(demo_port, 'GET', '/products/42')
            self.assertEqual(updated[1], {'value': 'new'})
            self.assertEqual(updated[2], 'MISS')
            self.assertEqual(call(demo_port, 'GET', '/demo/stats')[1]['originCallCount'], 2)
            time.sleep(.55)
            self.assertEqual(call(demo_port, 'GET', '/products/42')[2], 'MISS')
            self.assertEqual(call(demo_port, 'GET', '/demo/stats')[1]['originCallCount'], 3)
            self.assertEqual(call(demo_port, 'GET', '/products/42?cache=off')[2], 'MISS')
            self.assertEqual(call(demo_port, 'GET', '/demo/stats')[1]['originCallCount'], 4)
            # slow_source는 id=fail을 원본 장애 재현용으로 예약한다.
            for _ in range(2):
                self.assertEqual(call(demo_port, 'GET', '/products/fail')[0], 503)
            self.assertEqual(call(redis_port, 'GET', '/keys/product:fail')[0], 404)
            self.assertEqual(call(demo_port, 'GET', '/demo/stats')[1]['originCallCount'], 6)
            before = call(demo_port, 'GET', '/demo/stats')[1]['originCallCount']
            with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
                burst = list(pool.map(lambda _: call(demo_port, 'GET', '/products/burst'), range(8)))
            self.assertTrue(all(result[0] == 200 for result in burst))
            delta = call(demo_port, 'GET', '/demo/stats')[1]['originCallCount'] - before
            self.assertTrue(1 <= delta <= 8)
            print(f'동일 MISS 키 8개 동시 요청: 원본 {delta}회 (몰림 방지 없음)')
            large = 'v' * 1048576
            self.assertEqual(call(demo_port, 'PUT', '/products/max', {'value': large})[0], 200)
            self.assertEqual(call(demo_port, 'GET', '/products/max')[2], 'MISS')
            self.assertEqual(call(demo_port, 'GET', '/products/max')[2], 'HIT')
            self.assertEqual(call(redis_port, 'GET', '/keys/product:max')[1], {'value': large})
            escaped = '\"\\\n😀' * 1000
            self.assertEqual(call(demo_port, 'PUT', '/products/escaped', {'value': escaped})[0], 200)
            self.assertEqual(call(demo_port, 'GET', '/products/escaped'),
                             (200, {'value': escaped}, 'MISS'))
            self.assertEqual(call(demo_port, 'GET', '/products/escaped'),
                             (200, {'value': escaped}, 'HIT'))
            stop(redis)
            fallback = call(demo_port, 'GET', '/products/42')
            self.assertEqual(fallback, (200, {'value': 'new'}, 'MISS'))
            self.assertEqual(call(demo_port, 'PUT', '/products/42', {'value': 'offline'})[0], 503)
            self.assertEqual(call(demo_port, 'GET', '/products/42')[1], {'value': 'offline'})
        finally:
            if demo is not None:
                stop(demo)
            stop(redis)


if __name__ == '__main__':
    unittest.main(verbosity=2)
