"""협약 15.4 캐시 예제 검증. 빌드된 두 서버를 테스트용 포트로 직접 띄우고 끈다.
실행 : make 후 python3 tests/demo/test_demo.py
"""

import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "api"))
from api_client import TestRunner, send_request  # noqa: E402

MINI_REDIS_PORT = 16390
SAMPLE_API_PORT = 13090
CACHE_TTL_MS = 60000
STARTUP_SECONDS = 0.5
ROOT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")

mini_redis_address = ("127.0.0.1", MINI_REDIS_PORT)
sample_api_address = ("127.0.0.1", SAMPLE_API_PORT)
runner = TestRunner("test_demo")


def start_process(binary_name, *arguments):
    binary_path = os.path.join(ROOT_DIR, "build", binary_name)
    return subprocess.Popen([binary_path, *map(str, arguments)], stdout=subprocess.DEVNULL)


def origin_call_count():
    return send_request(sample_api_address, "GET", "/demo/stats")[1]["originCallCount"]


def get_product(product_id, query=""):
    return send_request(sample_api_address, "GET", f"/products/{product_id}{query}")


def test_hit_and_miss():
    calls_before = origin_call_count()
    status, body, headers = get_product(1)
    runner.check(status == 200 and headers.get("X-Cache") == "MISS", "첫 요청은 MISS")
    runner.check(origin_call_count() == calls_before + 1, "첫 요청은 원본을 한 번 조회")

    status, cached_body, headers = get_product(1)
    runner.check(headers.get("X-Cache") == "HIT" and cached_body == body, "두 번째 요청은 HIT")
    runner.check(origin_call_count() == calls_before + 1, "HIT이면 원본 조회 횟수 그대로")

    ttl_ms = send_request(mini_redis_address, "GET", "/keys/product%3A1/ttl")[1]["ttlMs"]
    runner.check(0 < ttl_ms <= CACHE_TTL_MS, "캐시에 TTL이 걸려 있음 (지나면 다시 MISS)")


def test_update_invalidates_cache():
    get_product(2)
    get_product(2)
    calls_before = origin_call_count()
    update = send_request(sample_api_address, "PUT", "/products/2", '{"name": "새 이름"}')
    runner.check(update[0] == 200, "상품 수정")

    status, body, headers = get_product(2)
    runner.check(headers.get("X-Cache") == "MISS", "수정 뒤 다음 조회는 MISS")
    runner.check(origin_call_count() == calls_before + 1, "수정 뒤 원본을 한 번 더 조회")
    runner.check(body.get("name") == "새 이름", "수정 뒤 새 값을 돌려줌")


def test_cache_off():
    calls_before = origin_call_count()
    get_product(3, "?cache=off")
    get_product(3, "?cache=off")
    runner.check(origin_call_count() == calls_before + 2, "cache=off는 매번 원본 조회")


def test_mini_redis_down(mini_redis_process):
    mini_redis_process.terminate()
    mini_redis_process.wait()
    status, body, _ = get_product(4)
    runner.check(status == 200 and body.get("id") == "4", "미니 레디스가 꺼져도 응답")


mini_redis_process = start_process("mini_redis", MINI_REDIS_PORT)
sample_api_process = start_process("sample_api", SAMPLE_API_PORT, MINI_REDIS_PORT)
time.sleep(STARTUP_SECONDS)
try:
    test_hit_and_miss()
    test_update_invalidates_cache()
    test_cache_off()
    test_mini_redis_down(mini_redis_process)
finally:
    mini_redis_process.terminate()
    sample_api_process.terminate()
runner.finish()
