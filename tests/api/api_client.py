"""외부 기능 테스트가 함께 쓰는 HTTP 도우미. 서버 주소만 바꾸면 세 서버에 똑같이 돈다. (협약 5장)"""

import http.client
import json
import sys
import urllib.parse

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 6380
REQUEST_TIMEOUT_SECONDS = 10


def read_server_address():
    """실행 인자로 '호스트:포트'를 받는다. 없으면 127.0.0.1:6380"""
    if len(sys.argv) < 2:
        return DEFAULT_HOST, DEFAULT_PORT
    host, port = sys.argv[1].split(":")
    return host, int(port)


def send_request(address, method, path, raw_body=None):
    """요청을 보내고 (상태 코드, 응답 JSON, 헤더)를 돌려준다. raw_body는 JSON 원문 문자열"""
    host, port = address
    connection = http.client.HTTPConnection(host, port, timeout=REQUEST_TIMEOUT_SECONDS)
    body = raw_body.encode("utf-8") if raw_body is not None else None
    connection.request(method, path, body=body)
    response = connection.getresponse()
    text = response.read().decode("utf-8")
    headers = dict(response.getheaders())
    connection.close()
    return response.status, (json.loads(text) if text else None), headers


def key_path(key, suffix=""):
    return "/keys/" + urllib.parse.quote(key, safe="") + suffix


class TestRunner:
    """CHECK처럼 실패를 세고 마지막에 결과를 출력한다"""

    def __init__(self, name):
        self.name = name
        self.failed_count = 0

    def check(self, condition, description):
        if not condition:
            self.failed_count += 1
            print(f"  실패 : {description}")

    def check_error(self, result, expected_status, expected_code, description):
        status, body, _ = result
        code = body.get("error", {}).get("code") if isinstance(body, dict) else None
        self.check(status == expected_status and code == expected_code,
                   f"{description} → {status} {code} (기대 {expected_status} {expected_code})")

    def finish(self):
        if self.failed_count > 0:
            print(f"{self.name} : {self.failed_count}개 실패")
            sys.exit(1)
        print(f"{self.name} : 모두 통과")
