#!/usr/bin/env python3
"""캐시 샘플 API 테스트 (협약 13장, 15.4 캐시 예제 검증, 15.5 캐시 예제 엣지 케이스).

미니 레디스를 끄는 경우까지 확인해야 해서, 이 테스트가 두 서버를 직접 띄우고 끈다.
실행 : python3 tests/demo/test_demo.py build/mini_redis build/sample_api
캐시 TTL은 실행 인자로 짧게(CACHE_TTL_MS_FOR_TEST) 줘서 TTL 만료를 빨리 확인한다.
"""

import concurrent.futures
import http.client
import json
import socket
import subprocess
import sys
import time
import unittest

REDIS_BIN = sys.argv.pop(1) if len(sys.argv) > 2 else "build/mini_redis"
DEMO_BIN = sys.argv.pop(1) if len(sys.argv) > 1 else "build/sample_api"
REDIS_PORT = 16391
DEMO_PORT = 13091
CACHE_TTL_MS_FOR_TEST = 1000
HOST = "127.0.0.1"
START_TIMEOUT_SECONDS = 5
REQUEST_TIMEOUT_SECONDS = 10

processes = {}


def wait_for_port(port):
    deadline = time.monotonic() + START_TIMEOUT_SECONDS
    while time.monotonic() < deadline:
        try:
            with socket.create_connection((HOST, port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError(f"포트 {port}의 서버가 뜨지 않았습니다")


def start_process(name, command, port):
    processes[name] = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    wait_for_port(port)


def stop_process(name):
    process = processes.pop(name, None)
    if process is not None:
        process.terminate()
        process.wait(timeout=START_TIMEOUT_SECONDS)


def request(port, method, path, body=None):
    connection = http.client.HTTPConnection(HOST, port, timeout=REQUEST_TIMEOUT_SECONDS)
    try:
        payload = json.dumps(body) if body is not None else None
        connection.request(method, path, payload)
        response = connection.getresponse()
        data = json.loads(response.read())
        return response.status, data, response.getheader("X-Cache")
    finally:
        connection.close()


def get_product(product_id, query=""):
    return request(DEMO_PORT, "GET", f"/products/{product_id}{query}")


def origin_call_count():
    return request(DEMO_PORT, "GET", "/demo/stats")[1]["originCallCount"]


def setUpModule():
    start_process("redis", [REDIS_BIN, str(REDIS_PORT)], REDIS_PORT)
    start_process("demo", [DEMO_BIN, str(DEMO_PORT), str(REDIS_PORT), str(CACHE_TTL_MS_FOR_TEST)],
                  DEMO_PORT)


def tearDownModule():
    stop_process("demo")
    stop_process("redis")


class CacheAsideTest(unittest.TestCase):
    """테스트 이름 앞 번호 순서대로 돈다. 미니 레디스를 끄는 테스트가 마지막이다."""

    def test_01_miss_then_hit(self):
        before = origin_call_count()
        status, first, cache = get_product(1)
        self.assertEqual((status, cache), (200, "MISS"))
        self.assertEqual(first, {"id": 1, "name": "product-1", "price": 100, "version": 1})
        self.assertEqual(origin_call_count(), before + 1)
        status, second, cache = get_product(1)
        self.assertEqual((status, cache, second), (200, "HIT", first))
        self.assertEqual(origin_call_count(), before + 1)

    def test_02_cache_reaches_mini_redis(self):
        """캐시 ON의 저장이 실제로 미니 레디스(직접 만든 해시 테이블)까지 갔는지 본다."""
        _, product, _ = get_product(2)
        status, body, _ = request(REDIS_PORT, "GET", "/keys/product%3A2")
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body["value"]), product)
        status, body, _ = request(REDIS_PORT, "GET", "/keys/product%3A2/ttl")
        self.assertTrue(0 < body["ttlMs"] <= CACHE_TTL_MS_FOR_TEST, body)

    def test_03_update_invalidates_cache(self):
        get_product(3)
        self.assertEqual(get_product(3)[2], "HIT")
        status, updated, _ = request(DEMO_PORT, "PUT", "/products/3", {"name": "새 이름", "price": 999})
        self.assertEqual(status, 200)
        self.assertEqual(updated["version"], 2)
        self.assertEqual(request(REDIS_PORT, "GET", "/keys/product%3A3")[0], 404)
        before = origin_call_count()
        status, product, cache = get_product(3)
        self.assertEqual((status, cache), (200, "MISS"))
        self.assertEqual((product["name"], product["price"]), ("새 이름", 999))
        self.assertEqual(origin_call_count(), before + 1)

    def test_04_ttl_expiry_makes_miss(self):
        get_product(4)
        self.assertEqual(get_product(4)[2], "HIT")
        time.sleep(CACHE_TTL_MS_FOR_TEST / 1000 + 0.2)
        self.assertEqual(get_product(4)[2], "MISS")

    def test_05_cache_off_always_uses_origin(self):
        get_product(5)
        before = origin_call_count()
        for _ in range(2):
            status, _, cache = get_product(5, "?cache=off")
            self.assertEqual((status, cache), (200, None))
        self.assertEqual(origin_call_count(), before + 2)

    def test_06_origin_failure_is_not_cached(self):
        before = origin_call_count()
        for _ in range(2):
            status, body, cache = get_product(0)
            self.assertEqual((status, body["error"]["code"], cache), (404, "PRODUCT_NOT_FOUND", None))
        self.assertEqual(origin_call_count(), before + 2)
        self.assertEqual(request(REDIS_PORT, "GET", "/keys/product%3A0")[0], 404)

    def test_07_bad_requests(self):
        self.assertEqual(get_product("abc")[1]["error"]["code"], "INVALID_PRODUCT_ID")
        self.assertEqual(request(DEMO_PORT, "GET", "/nothing")[0], 404)
        self.assertEqual(request(DEMO_PORT, "DELETE", "/products/1")[0], 405)
        status, body, _ = request(DEMO_PORT, "PUT", "/products/1", {"name": "x"})
        self.assertEqual((status, body["error"]["code"]), (400, "INVALID_BODY"))

    def test_08_stampede_is_observed(self):
        """같은 빗나감 키로 요청이 몰리면 원본이 여러 번 불린다. 막지 않고 확인만 한다 (협약 13.3)."""
        before = origin_call_count()
        with concurrent.futures.ThreadPoolExecutor(10) as executor:
            results = list(executor.map(lambda _: get_product(8)[0], range(10)))
        self.assertTrue(all(status == 200 for status in results))
        calls = origin_call_count() - before
        print(f"\n  [참고] 같은 빗나감 키 동시 요청 10개 → 원본 조회 {calls}번", file=sys.stderr)
        self.assertGreaterEqual(calls, 1)

    def test_09_works_when_mini_redis_is_down(self):
        stop_process("redis")
        started = time.monotonic()
        status, product, cache = get_product(9)
        self.assertEqual((status, product["id"], cache), (200, 9, "MISS"))
        self.assertLess(time.monotonic() - started, 3)
        status, updated, _ = request(DEMO_PORT, "PUT", "/products/9", {"name": "down", "price": 1})
        self.assertEqual((status, updated["name"]), (200, "down"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
