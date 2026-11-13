#!/usr/bin/env python3
"""서버 주소만 바꿔 공통 적용: python3 tests/api/test_api_edge.py http://127.0.0.1:6380"""
import concurrent.futures
import http.client
import json
import socket
import sys
import time
import unittest
import urllib.parse

BASE = sys.argv.pop(1) if len(sys.argv) > 1 else 'http://127.0.0.1:6380'
URL = urllib.parse.urlsplit(BASE)


def request(method, path, body=None, raw=False):
    connection = http.client.HTTPConnection(URL.hostname, URL.port, timeout=5)
    if body is not None and not raw:
        body = json.dumps(body, ensure_ascii=True)
    connection.request(method, path, body, {'Content-Type': 'application/json'})
    response = connection.getresponse()
    data = response.read()
    headers = dict(response.getheaders())
    status = response.status
    connection.close()
    return status, json.loads(data), headers


def key_path(key):
    return '/keys/' + urllib.parse.quote(key, safe='')


class ApiTest(unittest.TestCase):
    def setUp(self):
        request('POST', '/invalidate', {'prefix': 'test:'})

    def check_error(self, method, path, body, status, code, raw=False):
        actual, data, headers = request(method, path, body, raw)
        self.assertEqual(actual, status, data)
        self.assertEqual(data['error']['code'], code)
        self.assertRegex(data['error']['message'], r'^\[[a-z_]+\] .+')
        self.assertEqual(headers['Content-Type'], 'application/json; charset=utf-8')

    def test_crud_ttl_invalidation_stats(self):
        path = key_path('test:상품/😀?% :42')
        self.assertEqual(request('PUT', path, {'value': ''})[:2], (200, {'created': True}))
        self.assertEqual(request('GET', path)[:2], (200, {'value': ''}))
        self.assertEqual(request('PUT', path, {'value': 'new', 'ttlMs': 5000})[1],
                         {'created': False})
        self.assertEqual(request('PUT', path + '/ttl', {'ttlMs': 4000})[1], {'applied': True})
        self.assertTrue(0 < request('GET', path + '/ttl')[1]['ttlMs'] <= 4000)
        request('PUT', path, {'value': 'forever'})
        self.assertEqual(request('GET', path + '/ttl')[1], {'ttlMs': -1})
        self.assertEqual(request('POST', '/invalidate', {'prefix': 'test:'})[1],
                         {'deletedCount': 1})
        self.check_error('GET', path, None, 404, 'KEY_NOT_FOUND')
        self.assertEqual(request('DELETE', path)[1], {'deleted': False})
        self.assertEqual(request('PUT', path + '/ttl', {'ttlMs': 100})[1], {'applied': False})
        self.check_error('GET', path + '/ttl', None, 404, 'KEY_NOT_FOUND')
        stats = request('GET', '/stats')[1]
        self.assertEqual(set(stats), {'size', 'capacity', 'hitCount', 'missCount', 'expiredCount'})

    def test_validation_preserves_value_and_deadline(self):
        path = key_path('test:preserve')
        request('PUT', path, {'value': 'old', 'ttlMs': 10000})
        cases = [('{', 'INVALID_JSON'), ('{\"value\":\"a\x00b\"}', 'INVALID_JSON'), ('[]', 'INVALID_BODY'), ('{}', 'INVALID_BODY'),
                 ('{"value":"x","extra":1}', 'INVALID_BODY'),
                 ('{"value":5}', 'INVALID_VALUE'), ('{"value":"\\u0000"}', 'INVALID_VALUE'),
                 ('{"value":"x","ttlMs":100,"ttlMs":200}', 'INVALID_BODY'),
                 ('{"value":"x","ttlMs":100,"ttl\\u004ds":200}', 'INVALID_BODY'),
                 ('{"value":"x","value":"y"}', 'INVALID_BODY'),
                 ('{"value":"x"} trailing', 'INVALID_JSON')]
        for value in ['0', '-1', '1.5', '"1"', 'null', '2147483648', '1e999', '1.00000000000000000001', '2147483647.00000001']:
            cases.append(('{"value":"x","ttlMs":' + value + '}', 'INVALID_TTL'))
        for body, code in cases:
            with self.subTest(body=body):
                self.check_error('PUT', path, body, 400, code, True)
                self.assertEqual(request('GET', path)[1], {'value': 'old'})
                self.assertTrue(0 < request('GET', path + '/ttl')[1]['ttlMs'] <= 10000)
        self.assertEqual(request('PUT', path, '{"value":"x","ttlMs":1.0}', True)[0], 200)
        self.check_error('POST', '/invalidate', {'prefix': ''}, 400, 'INVALID_PREFIX')
        self.check_error('PUT', path, None, 400, 'INVALID_JSON')

    def test_limits_routing_and_encoding(self):
        self.check_error('GET', '/keys/', None, 400, 'INVALID_KEY')
        for key in ['%ZZ', '%00', 'a' * 257, '%FF']:
            self.check_error('GET', '/keys/' + key, None, 400, 'INVALID_KEY')
        path = key_path('k' * 256)
        self.assertEqual(request('PUT', path, {'value': 'v'})[0], 200)
        request('DELETE', path)
        self.check_error('GET', '/unknown', None, 404, 'ROUTE_NOT_FOUND')
        self.check_error('POST', '/stats', None, 405, 'METHOD_NOT_ALLOWED')
        self.check_error('PATCH', '/keys/test:a', None, 405, 'METHOD_NOT_ALLOWED')
        large = 'v' * 1048576
        self.assertEqual(request('PUT', '/keys/test:large', {'value': large})[0], 200)
        self.assertEqual(request('GET', '/keys/test:large')[1]['value'], large)
        self.check_error('PUT', '/keys/test:large', {'value': large + 'v'}, 413, 'VALUE_TOO_LARGE')
        # 상한 초과는 본문을 보내기 전에 거부해야 한다. 본문을 전송하지 않아도 413.
        with socket.create_connection((URL.hostname, URL.port)) as connection:
            connection.sendall(b'PUT /keys/test:large HTTP/1.1\r\nHost: local\r\n'
                               b'Content-Length: 2097153\r\n\r\n')
            response = http.client.HTTPResponse(connection)
            response.begin()
            self.assertEqual(response.status, 413)
            self.assertEqual(json.loads(response.read())['error']['code'], 'PAYLOAD_TOO_LARGE')

    def test_expiry_and_sweeper(self):
        path = '/keys/test:expire'
        request('PUT', path, {'value': 'v', 'ttlMs': 200})
        self.assertEqual(request('GET', path)[0], 200)
        deadline = time.monotonic() + 3
        while request('GET', path)[0] == 200:
            self.assertLess(time.monotonic(), deadline)
            time.sleep(.02)
        self.assertEqual(request('DELETE', path)[1], {'deleted': False})
        before = request('GET', '/stats')[1]['expiredCount']
        request('PUT', path, {'value': 'unread', 'ttlMs': 20})
        time.sleep(1.2)
        # stats 자체도 정리하므로 물리적 주기 정리는 별도 sweeper 단위 테스트에서 확인한다.
        self.assertGreater(request('GET', '/stats')[1]['expiredCount'], before)

    def test_concurrent_resize_and_delete(self):
        stable = '/keys/test:stable'
        request('PUT', stable, {'value': 'stable'})
        def worker(index):
            path = '/keys/test:item' + str(index)
            self.assertEqual(request('PUT', path, {'value': str(index)})[0], 200)
            self.assertEqual(request('GET', stable)[1], {'value': 'stable'})
            return path, str(index)
        with concurrent.futures.ThreadPoolExecutor(max_workers=16) as pool:
            pairs = list(pool.map(worker, range(600)))
            for path, value in pairs:
                self.assertEqual(request('GET', path)[1], {'value': value})
            deleted = list(pool.map(lambda _: request('DELETE', stable)[1]['deleted'], range(40)))
            self.assertEqual(sum(deleted), 1)
            def mixed(index):
                if index % 2:
                    return request('DELETE', '/keys/test:mixed')[0]
                return request('PUT', '/keys/test:mixed', {'value': str(index)})[0]
            self.assertEqual(set(pool.map(mixed, range(100))), {200})
        request('PUT', '/keys/test:mixed', {'value': 'final'})
        self.assertEqual(request('GET', '/keys/test:mixed')[1], {'value': 'final'})

    def test_refresh_during_sweep(self):
        for index in range(20):
            path = '/keys/test:refresh' + str(index)
            request('PUT', path, {'value': 'old', 'ttlMs': 1})
            time.sleep(.002)
            request('PUT', path, {'value': 'fresh'})
        time.sleep(1.1)
        for index in range(20):
            self.assertEqual(request('GET', '/keys/test:refresh' + str(index))[1], {'value': 'fresh'})

    def test_disconnect_and_fragmented_http(self):
        with socket.create_connection((URL.hostname, URL.port)) as connection:
            connection.sendall(b'PUT /keys/test:fragment HTTP/1.1\r\nHost: local\r\nContent-Len')
            time.sleep(.01)
            connection.sendall(b'gth: 13\r\n\r\n{"value":"v"}')
            data = b''
            while True:
                part = connection.recv(4096)
                if not part:
                    break
                data += part
            self.assertIn(b'200 OK', data)
        for _ in range(10):
            connection = socket.create_connection((URL.hostname, URL.port))
            connection.sendall(b'GET /keys/test:large HTTP/1.1\r\nHost: local\r\n\r\n')
            connection.close()
        self.assertEqual(request('GET', '/stats')[0], 200)


if __name__ == '__main__':
    unittest.main(verbosity=2)
