#!/usr/bin/env python3
"""미니 레디스 외부 기능 테스트 (협약 15.2 API, 15.3 동시성, 15.5 API 엣지 케이스).

서버 주소만 바꾸면 세 구현에 똑같이 돌릴 수 있다. 서버는 먼저 띄워 둔다.
실행 : python3 tests/api/test_api.py [http://127.0.0.1:6380]
표준 라이브러리만 쓴다.
"""

import concurrent.futures
import http.client
import json
import socket
import sys
import time
import unittest
import urllib.parse

BASE_URL = sys.argv.pop(1) if len(sys.argv) > 1 else "http://127.0.0.1:6380"
ADDRESS = urllib.parse.urlsplit(BASE_URL)
RUN_ID = str(int(time.time() * 1000))
REQUEST_TIMEOUT_SECONDS = 20
SERVER_REQUEST_DEADLINE_SECONDS = 10    # 내 서버의 HTTP_REQUEST_TIMEOUT_MS
SERVER_WORKER_COUNT = 8                 # 내 서버의 WORKER_COUNT
EXPIRY_WAIT_LIMIT_SECONDS = 3
EXPIRY_POLL_SECONDS = 0.05
MAX_KEY_BYTES = 256
MAX_VALUE_BYTES = 1048576
MAX_BODY_BYTES = 2097152
JSON_CONTENT_TYPE = "application/json; charset=utf-8"
WORKER_THREADS = 16


def make_key(name):
    """이전 실행이나 다른 테스트와 섞이지 않게 실행마다 다른 접두사를 붙인다."""
    return f"sktest:{RUN_ID}:{name}"


def key_path(key, suffix=""):
    return "/keys/" + urllib.parse.quote(key, safe="") + suffix


def request(method, path, body=None):
    """body가 str이면 원문 그대로, dict면 JSON으로 보낸다. (상태 코드, JSON, 헤더)를 돌려준다."""
    connection = http.client.HTTPConnection(ADDRESS.hostname, ADDRESS.port,
                                            timeout=REQUEST_TIMEOUT_SECONDS)
    if isinstance(body, dict):
        body = json.dumps(body, ensure_ascii=False)
    if isinstance(body, str):
        body = body.encode("utf-8")
    try:
        connection.request(method, path, body, {"Content-Type": "application/json"})
        response = connection.getresponse()
        data = response.read()
        return response.status, json.loads(data), dict(response.getheaders())
    finally:
        connection.close()


def send_raw(raw_bytes):
    """HTTP 클라이언트 라이브러리를 거치지 않고 원문을 그대로 보낸다."""
    with socket.create_connection((ADDRESS.hostname, ADDRESS.port),
                                  timeout=REQUEST_TIMEOUT_SECONDS) as connection:
        connection.sendall(raw_bytes)
        chunks = []
        while True:
            chunk = connection.recv(65536)
            if not chunk:
                break
            chunks.append(chunk)
    return b"".join(chunks).decode("utf-8", errors="replace")


class ApiTestCase(unittest.TestCase):
    def assert_ok(self, result, expected_body):
        status, body, headers = result
        self.assertEqual(status, 200, body)
        self.assertEqual(body, expected_body)
        self.assertEqual(headers.get("Content-Type"), JSON_CONTENT_TYPE)

    def assert_error(self, result, expected_status, expected_code):
        status, body, headers = result
        self.assertEqual(status, expected_status, body)
        self.assertEqual(body["error"]["code"], expected_code)
        self.assertRegex(body["error"]["message"], r"^\[[a-z_]+\] .+")
        self.assertEqual(headers.get("Content-Type"), JSON_CONTENT_TYPE)


class BasicFlowTest(ApiTestCase):
    def test_set_get_overwrite_delete(self):
        path = key_path(make_key("basic"))
        self.assert_ok(request("PUT", path, {"value": "1"}), {"created": True})
        self.assert_ok(request("GET", path), {"value": "1"})
        self.assert_ok(request("PUT", path, {"value": "2"}), {"created": False})
        self.assert_ok(request("GET", path), {"value": "2"})
        self.assert_ok(request("DELETE", path), {"deleted": True})
        self.assert_error(request("GET", path), 404, "KEY_NOT_FOUND")
        self.assert_ok(request("DELETE", path), {"deleted": False})
        self.assert_ok(request("PUT", path, {"value": "again"}), {"created": True})

    def test_empty_value_is_not_missing(self):
        path = key_path(make_key("empty"))
        self.assert_ok(request("PUT", path, {"value": ""}), {"created": True})
        self.assert_ok(request("GET", path), {"value": ""})

    def test_value_round_trip_with_escapes(self):
        path = key_path(make_key("escape"))
        value = 'quote" back\\ newline\n tab\t 한글 😀 \u0001'
        self.assert_ok(request("PUT", path, {"value": value}), {"created": True})
        self.assert_ok(request("GET", path), {"value": value})

    def test_special_keys_with_url_encoding(self):
        for index, name in enumerate(["한글", "😀", "a/b", "q?x", "100%", "공 백", "user:42:profile"]):
            path = key_path(make_key(f"special{index}:{name}"))
            self.assert_ok(request("PUT", path, {"value": name}), {"created": True})
            self.assert_ok(request("GET", path), {"value": name})
            self.assert_ok(request("DELETE", path), {"deleted": True})

    def test_ttl_endpoints(self):
        path = key_path(make_key("ttl"))
        self.assert_error(request("GET", path + "/ttl"), 404, "KEY_NOT_FOUND")
        self.assert_ok(request("PUT", path + "/ttl", {"ttlMs": 1000}), {"applied": False})
        request("PUT", path, {"value": "v"})
        self.assert_ok(request("GET", path + "/ttl"), {"ttlMs": -1})
        self.assert_ok(request("PUT", path + "/ttl", {"ttlMs": 100000}), {"applied": True})
        status, body, _ = request("GET", path + "/ttl")
        self.assertEqual(status, 200)
        self.assertTrue(0 < body["ttlMs"] <= 100000, body)
        request("PUT", path, {"value": "no ttl"})
        self.assert_ok(request("GET", path + "/ttl"), {"ttlMs": -1})

    def test_exists_and_health(self):
        path = key_path(make_key("exists"))
        self.assert_ok(request("GET", path + "/exists"), {"exists": False})
        request("PUT", path, {"value": "v"})
        self.assert_ok(request("GET", path + "/exists"), {"exists": True})
        self.assert_ok(request("GET", "/health"), {"status": "ok"})

    def test_invalidate_by_prefix(self):
        prefix = make_key("product:")
        for index in range(3):
            request("PUT", key_path(f"{prefix}{index}"), {"value": str(index)})
        other = key_path(make_key("other:1"))
        request("PUT", other, {"value": "keep"})
        self.assert_ok(request("POST", "/invalidate", {"prefix": prefix}), {"deletedCount": 3})
        self.assert_error(request("GET", key_path(f"{prefix}0")), 404, "KEY_NOT_FOUND")
        self.assert_ok(request("GET", other), {"value": "keep"})
        self.assert_ok(request("POST", "/invalidate", {"prefix": prefix}), {"deletedCount": 0})

    def test_stats_shape(self):
        status, body, _ = request("GET", "/stats")
        self.assertEqual(status, 200)
        self.assertEqual(set(body), {"size", "capacity", "hitCount", "missCount", "expiredCount"})
        before = body
        path = key_path(make_key("stats"))
        request("PUT", path, {"value": "v"})
        request("GET", path)
        request("GET", key_path(make_key("stats-missing")))
        _, after, _ = request("GET", "/stats")
        self.assertGreaterEqual(after["hitCount"], before["hitCount"] + 1)
        self.assertGreaterEqual(after["missCount"], before["missCount"] + 1)
        self.assertGreaterEqual(after["capacity"], after["size"])


class ExpiryTest(ApiTestCase):
    def test_key_expires(self):
        path = key_path(make_key("expire"))
        self.assert_ok(request("PUT", path, {"value": "short", "ttlMs": 300}), {"created": True})
        self.assert_ok(request("GET", path), {"value": "short"})
        deadline = time.monotonic() + EXPIRY_WAIT_LIMIT_SECONDS
        status = 200
        while status == 200 and time.monotonic() < deadline:
            time.sleep(EXPIRY_POLL_SECONDS)
            status = request("GET", path)[0]
        self.assertEqual(status, 404)
        self.assert_error(request("GET", path + "/ttl"), 404, "KEY_NOT_FOUND")
        self.assert_ok(request("PUT", path + "/ttl", {"ttlMs": 1000}), {"applied": False})
        self.assert_ok(request("DELETE", path), {"deleted": False})
        self.assert_ok(request("PUT", path, {"value": "new"}), {"created": True})


class ValidationTest(ApiTestCase):
    def setUp(self):
        self.path = key_path(make_key(f"validation:{self._testMethodName}"))
        request("PUT", self.path, {"value": "keep", "ttlMs": 100000})

    def assert_kept(self):
        self.assert_ok(request("GET", self.path), {"value": "keep"})
        status, body, _ = request("GET", self.path + "/ttl")
        self.assertEqual(status, 200)
        self.assertTrue(90000 < body["ttlMs"] <= 100000, body)

    def test_invalid_json_and_body(self):
        cases = [
            ('{"value": ', "INVALID_JSON"),
            ("", "INVALID_JSON"),
            ('{"value":"x"} trailing', "INVALID_JSON"),
            ('{"value":"\\ud800"}', "INVALID_JSON"),
            ('["a"]', "INVALID_BODY"),
            ("123", "INVALID_BODY"),
            ("{}", "INVALID_BODY"),
            ('{"ttlMs": 1000}', "INVALID_BODY"),
            ('{"value": "x", "extra": 1}', "INVALID_BODY"),
            ('{"value": "x", "ttlMs": 100, "ttlMs": 200}', "INVALID_BODY"),
            ('{"value": "x", "ttlMs": 100, "ttl\\u004ds": 200}', "INVALID_BODY"),
            ('{"value": 1}', "INVALID_VALUE"),
            ('{"value": null}', "INVALID_VALUE"),
            ('{"value": "a\\u0000b"}', "INVALID_VALUE"),
            ("{" + ",".join(f'"f{index}": 0' for index in range(100000)) + "}", "INVALID_BODY"),
        ]
        for raw_body, code in cases:
            with self.subTest(body=raw_body):
                self.assert_error(request("PUT", self.path, raw_body), 400, code)
        self.assert_kept()

    def test_ttl_validation(self):
        for raw_ttl in ["1.5", '"1"', "0", "-1", "null", "2147483648", "true",
                        "2147483647.00000001", "1e400", "[]"]:
            with self.subTest(ttl=raw_ttl):
                body = '{"value": "bad", "ttlMs": ' + raw_ttl + "}"
                self.assert_error(request("PUT", self.path, body), 400, "INVALID_TTL")
                self.assert_error(request("PUT", self.path + "/ttl", '{"ttlMs": ' + raw_ttl + "}"),
                                  400, "INVALID_TTL")
        self.assert_error(request("PUT", self.path + "/ttl", "{}"), 400, "INVALID_BODY")
        self.assert_kept()

    def test_accepted_integer_forms(self):
        path = key_path(make_key("ttl-forms"))
        for raw_ttl in ["1.0", "1e3", "2147483647", "2147483647.000"]:
            with self.subTest(ttl=raw_ttl):
                status, _, _ = request("PUT", path, '{"value": "ok", "ttlMs": ' + raw_ttl + "}")
                self.assertEqual(status, 200)

    def test_key_validation(self):
        self.assert_error(request("GET", "/keys/"), 400, "INVALID_KEY")
        self.assert_error(request("GET", "/keys/a%00b"), 400, "INVALID_KEY")
        self.assert_error(request("GET", "/keys/%zz"), 400, "INVALID_KEY")
        self.assert_error(request("GET", "/keys/abc%"), 400, "INVALID_KEY")
        self.assert_error(request("GET", "/keys/%ff"), 400, "INVALID_KEY")
        self.assert_error(request("PUT", key_path("k" * (MAX_KEY_BYTES + 1)), {"value": "x"}),
                          400, "INVALID_KEY")
        key_256 = "k" * MAX_KEY_BYTES
        self.assert_ok(request("PUT", key_path(key_256), {"value": "x"}), {"created": True})
        request("DELETE", key_path(key_256))

    def test_prefix_validation(self):
        self.assert_error(request("POST", "/invalidate", {"prefix": ""}), 400, "INVALID_PREFIX")
        self.assert_error(request("POST", "/invalidate", {"prefix": 1}), 400, "INVALID_PREFIX")
        self.assert_error(request("POST", "/invalidate", "{}"), 400, "INVALID_BODY")
        self.assert_error(request("POST", "/invalidate", "nope"), 400, "INVALID_JSON")
        self.assert_kept()

    def test_routes_and_methods(self):
        self.assert_error(request("GET", "/nothing"), 404, "ROUTE_NOT_FOUND")
        self.assert_error(request("GET", self.path + "/unknown"), 404, "ROUTE_NOT_FOUND")
        self.assert_error(request("POST", self.path), 405, "METHOD_NOT_ALLOWED")
        self.assert_error(request("PATCH", self.path), 405, "METHOD_NOT_ALLOWED")
        self.assert_error(request("DELETE", self.path + "/ttl"), 405, "METHOD_NOT_ALLOWED")
        self.assert_error(request("GET", "/invalidate"), 405, "METHOD_NOT_ALLOWED")
        self.assert_error(request("POST", "/stats"), 405, "METHOD_NOT_ALLOWED")

    def test_value_size_limits(self):
        path = key_path(make_key("large"))
        max_value = "v" * MAX_VALUE_BYTES
        self.assert_ok(request("PUT", path, {"value": max_value}), {"created": True})
        self.assert_ok(request("GET", path), {"value": max_value})
        self.assert_error(request("PUT", path, {"value": max_value + "v"}), 413, "VALUE_TOO_LARGE")
        self.assert_ok(request("GET", path), {"value": max_value})
        request("DELETE", path)

    def test_payload_too_large(self):
        body = '{"value": "' + "v" * MAX_BODY_BYTES + '"}'
        self.assert_error(request("PUT", self.path, body), 413, "PAYLOAD_TOO_LARGE")
        self.assert_kept()

    def test_chunked_body_is_rejected(self):
        raw = (f"PUT {self.path} HTTP/1.1\r\nHost: test\r\nTransfer-Encoding: chunked\r\n\r\n"
               "5\r\nhello\r\n0\r\n\r\n").encode()
        response = send_raw(raw)
        self.assertTrue(response.startswith("HTTP/1.1 400"), response)
        self.assertIn("INVALID_BODY", response)
        self.assert_kept()


class RobustnessTest(ApiTestCase):
    def test_client_disconnects_mid_request(self):
        for partial in [b"PUT /keys/a HTTP/1.1\r\nContent-Length: 100\r\n\r\n{\"val",
                        b"GET /keys/a HT", b""]:
            with socket.create_connection((ADDRESS.hostname, ADDRESS.port)) as connection:
                connection.sendall(partial)
        self.assert_ok(request("GET", "/health"), {"status": "ok"})

    def test_slow_clients_do_not_block_forever(self):
        """모든 워커를 아무것도 보내지 않는 연결이 붙잡아도, 요청 마감(10초) 뒤에는 다시 처리된다."""
        idle_connections = [socket.create_connection((ADDRESS.hostname, ADDRESS.port))
                             for _ in range(SERVER_WORKER_COUNT)]
        try:
            started = time.monotonic()
            self.assert_ok(request("GET", "/health"), {"status": "ok"})
            self.assertLess(time.monotonic() - started, SERVER_REQUEST_DEADLINE_SECONDS + 5)
        finally:
            for connection in idle_connections:
                connection.close()

    def test_malformed_request_line(self):
        response = send_raw(b"HELLO\r\n\r\n")
        self.assertTrue(response.startswith("HTTP/1.1 400"), response)
        self.assert_ok(request("GET", "/health"), {"status": "ok"})


class ConcurrencyTest(ApiTestCase):
    def test_value_survives_concurrent_writes_and_rehash(self):
        stable = key_path(make_key("stable"))
        request("PUT", stable, {"value": "A"})
        write_count = 400

        def write(index):
            return request("PUT", key_path(make_key(f"bulk:{index}")), {"value": str(index)})[0]

        def read_stable(_):
            return request("GET", stable)[:2]

        with concurrent.futures.ThreadPoolExecutor(WORKER_THREADS) as executor:
            writes = [executor.submit(write, index) for index in range(write_count)]
            reads = [executor.submit(read_stable, index) for index in range(write_count)]
            write_statuses = [future.result() for future in writes]
            read_results = [future.result() for future in reads]
        self.assertTrue(all(status == 200 for status in write_statuses))
        self.assertTrue(all(result == (200, {"value": "A"}) for result in read_results))
        for index in range(write_count):
            self.assertEqual(request("GET", key_path(make_key(f"bulk:{index}")))[1],
                             {"value": str(index)})

    def test_concurrent_delete_succeeds_once(self):
        path = key_path(make_key("delete-once"))
        for _ in range(5):
            request("PUT", path, {"value": "v"})
            with concurrent.futures.ThreadPoolExecutor(WORKER_THREADS) as executor:
                results = list(executor.map(lambda _: request("DELETE", path)[1], range(20)))
            self.assertEqual(sum(1 for body in results if body == {"deleted": True}), 1)

    def test_concurrent_puts_leave_one_whole_value(self):
        path = key_path(make_key("same-key"))
        values = [f"value-{index}-" + "x" * 1000 for index in range(50)]
        with concurrent.futures.ThreadPoolExecutor(WORKER_THREADS) as executor:
            list(executor.map(lambda value: request("PUT", path, {"value": value}), values))
        self.assertIn(request("GET", path)[1]["value"], values)

    def test_mixed_put_and_delete_stay_consistent(self):
        path = key_path(make_key("mixed"))

        def operate(index):
            if index % 2 == 0:
                return request("PUT", path, {"value": "v"})[0]
            return request("DELETE", path)[0]

        with concurrent.futures.ThreadPoolExecutor(WORKER_THREADS) as executor:
            statuses = list(executor.map(operate, range(200)))
        self.assertTrue(all(status == 200 for status in statuses))
        status, body, _ = request("GET", path)
        self.assertIn((status, body), [(200, {"value": "v"}), (404, body)])
        self.assert_ok(request("GET", "/health"), {"status": "ok"})


if __name__ == "__main__":
    unittest.main(verbosity=1)
