"""협약 15.2 외부 기능 테스트와 15.5의 API 엣지 케이스.
실행 : python3 tests/api/test_api.py [호스트:포트]   (서버를 먼저 띄운다)
"""

import socket
import time

from api_client import TestRunner, key_path, read_server_address, send_request

EXPIRY_WAIT_LIMIT_SECONDS = 3
EXPIRY_POLL_SECONDS = 0.05
RUN_ID = str(int(time.time() * 1000))   # 이전 실행의 키와 섞이지 않게 붙인다

address = read_server_address()
runner = TestRunner("test_api")


def make_key(name):
    return f"test:{RUN_ID}:{name}"


def put_key(key, raw_body):
    return send_request(address, "PUT", key_path(key), raw_body)


def get_key(key):
    return send_request(address, "GET", key_path(key))


def test_basic_flow():
    key = make_key("basic")
    runner.check(put_key(key, '{"value": "1"}')[:2] == (200, {"created": True}), "새로 저장")
    runner.check(get_key(key)[:2] == (200, {"value": "1"}), "저장 후 조회")
    runner.check(put_key(key, '{"value": "2"}')[:2] == (200, {"created": False}), "덮어쓰기")
    runner.check(get_key(key)[:2] == (200, {"value": "2"}), "덮어쓴 값 조회")

    delete_result = send_request(address, "DELETE", key_path(key))
    runner.check(delete_result[:2] == (200, {"deleted": True}), "삭제")
    runner.check_error(get_key(key), 404, "KEY_NOT_FOUND", "삭제 후 재조회")
    second_delete = send_request(address, "DELETE", key_path(key))
    runner.check(second_delete[:2] == (200, {"deleted": False}), "같은 DELETE 두 번")


def test_empty_value_and_missing_field():
    key = make_key("empty")
    runner.check(put_key(key, '{"value": ""}')[0] == 200, "빈 문자열 값 저장")
    runner.check(get_key(key)[:2] == (200, {"value": ""}), "빈 값은 없음과 구분")
    runner.check_error(put_key(key, '{"ttlMs": 1000}'), 400, "INVALID_BODY", "value 누락")
    runner.check_error(put_key(key, '{"value": 1}'), 400, "INVALID_VALUE", "value가 숫자")
    runner.check_error(put_key(key, '{"value": "a\\u0000b"}'), 400, "INVALID_VALUE", "value NUL")


def test_body_validation():
    key = make_key("validation")
    put_key(key, '{"value": "keep"}')
    runner.check_error(put_key(key, '{"value": '), 400, "INVALID_JSON", "잘못된 JSON")
    runner.check_error(put_key(key, ""), 400, "INVALID_JSON", "본문 없는 PUT")
    runner.check_error(put_key(key, '["a"]'), 400, "INVALID_BODY", "최상위가 배열")
    runner.check_error(put_key(key, '{"value": "x", "extra": 1}'), 400, "INVALID_BODY",
                       "정의되지 않은 필드")
    runner.check(get_key(key)[:2] == (200, {"value": "keep"}), "거부된 요청은 값을 바꾸지 않음")


def test_ttl_validation():
    key = make_key("ttl-validation")
    runner.check(put_key(key, '{"value": "x", "ttlMs": 1.0}')[0] == 200, "ttlMs 1.0 허용")
    for raw_ttl in ["1.5", '"1"', "0", "-1", "null", "2147483648"]:
        result = put_key(key, '{"value": "x", "ttlMs": ' + raw_ttl + "}")
        runner.check_error(result, 400, "INVALID_TTL", f"ttlMs {raw_ttl} 거부")


def test_duplicate_field():
    key = make_key("duplicate")
    put_key(key, '{"value": "keep", "ttlMs": 100000}')
    result = put_key(key, '{"value": "x", "ttlMs": 100, "ttlMs": 200}')
    runner.check_error(result, 400, "INVALID_BODY", "ttlMs 중복")
    escaped_result = put_key(key, '{"value": "x", "ttlMs": 100, "ttl\\u004ds": 200}')
    runner.check_error(escaped_result, 400, "INVALID_BODY", "이스케이프한 이름으로 중복")
    runner.check(get_key(key)[:2] == (200, {"value": "keep"}), "중복 거부 후 기존 값 유지")
    ttl_result = send_request(address, "GET", key_path(key, "/ttl"))
    runner.check(ttl_result[0] == 200 and ttl_result[1]["ttlMs"] > 1000, "중복 거부 후 만료 유지")


def test_ttl_endpoints():
    key = make_key("ttl")
    put_key(key, '{"value": "x"}')
    runner.check(send_request(address, "GET", key_path(key, "/ttl"))[:2] == (200, {"ttlMs": -1}),
                 "만료 없음은 -1")
    applied = send_request(address, "PUT", key_path(key, "/ttl"), '{"ttlMs": 100000}')
    runner.check(applied[:2] == (200, {"applied": True}), "TTL 걸기")
    ttl_ms = send_request(address, "GET", key_path(key, "/ttl"))[1]["ttlMs"]
    runner.check(0 < ttl_ms <= 100000, "남은 TTL 조회")

    put_key(key, '{"value": "y"}')
    runner.check(send_request(address, "GET", key_path(key, "/ttl"))[1] == {"ttlMs": -1},
                 "ttlMs 없이 덮어쓰면 만료 해제")
    missing = send_request(address, "PUT", key_path(make_key("none"), "/ttl"), '{"ttlMs": 100}')
    runner.check(missing[:2] == (200, {"applied": False}), "없는 키에 TTL은 false")
    runner.check_error(send_request(address, "GET", key_path(make_key("none"), "/ttl")),
                       404, "KEY_NOT_FOUND", "없는 키의 TTL 조회")


def test_expiry():
    key = make_key("expiry")
    put_key(key, '{"value": "soon", "ttlMs": 300}')
    runner.check(get_key(key)[0] == 200, "저장 직후 조회 성공")
    deadline = time.time() + EXPIRY_WAIT_LIMIT_SECONDS
    status = 200
    while status == 200 and time.time() < deadline:
        time.sleep(EXPIRY_POLL_SECONDS)
        status = get_key(key)[0]
    runner.check(status == 404, "시간이 지나면 404")


def test_invalidate():
    prefix = make_key("product:")
    for number in range(3):
        put_key(prefix + str(number), '{"value": "p"}')
    other_key = make_key("user:1")
    put_key(other_key, '{"value": "u"}')

    result = send_request(address, "POST", "/invalidate", '{"prefix": "' + prefix + '"}')
    runner.check(result[:2] == (200, {"deletedCount": 3}), "접두사 무효화")
    runner.check_error(get_key(prefix + "0"), 404, "KEY_NOT_FOUND", "무효화된 키 조회")
    runner.check(get_key(other_key)[0] == 200, "다른 접두사는 남음")
    runner.check_error(send_request(address, "POST", "/invalidate", '{"prefix": ""}'),
                       400, "INVALID_PREFIX", "빈 접두사")


def test_keys():
    runner.check_error(get_key(""), 400, "INVALID_KEY", "빈 키")
    runner.check(put_key("k" * 256, '{"value": "x"}')[0] == 200, "256바이트 키")
    runner.check_error(put_key("k" * 257, '{"value": "x"}'), 400, "INVALID_KEY", "257바이트 키")
    runner.check_error(send_request(address, "GET", "/keys/a%00b"), 400, "INVALID_KEY", "NUL 키")
    runner.check_error(send_request(address, "GET", "/keys/%zz"), 400, "INVALID_KEY", "잘못된 인코딩")

    special_key = make_key("한글 😀/?%:")
    runner.check(put_key(special_key, '{"value": "special"}')[0] == 200, "특수 문자 키 저장")
    runner.check(get_key(special_key)[:2] == (200, {"value": "special"}), "특수 문자 키 조회")


def test_value_size():
    key = make_key("size")
    max_value = "v" * 1048576
    runner.check(put_key(key, '{"value": "' + max_value + '"}')[0] == 200, "1MiB 값")
    runner.check_error(put_key(key, '{"value": "' + max_value + 'v"}'), 413, "VALUE_TOO_LARGE",
                       "1MiB 초과 값")


def test_payload_too_large():
    """서버는 본문을 읽지 않고 413을 돌려준다 (협약 11.5). 그래서 헤더만 보내고 응답을 읽는다"""
    request_head = (f"PUT {key_path(make_key('payload'))} HTTP/1.1\r\n"
                    "Content-Length: 2097153\r\n\r\n")
    with socket.create_connection(address, timeout=10) as connection:
        connection.sendall(request_head.encode("utf-8"))
        response_text = connection.recv(4096).decode("utf-8")
    is_413 = response_text.startswith("HTTP/1.1 413") and "PAYLOAD_TOO_LARGE" in response_text
    runner.check(is_413, "2MiB 초과 본문")


def test_routes_and_stats():
    runner.check_error(send_request(address, "GET", "/nothing"), 404, "ROUTE_NOT_FOUND", "없는 경로")
    runner.check_error(send_request(address, "POST", key_path("a")), 405, "METHOD_NOT_ALLOWED",
                       "허용하지 않는 메서드")
    runner.check_error(send_request(address, "GET", "/invalidate"), 405, "METHOD_NOT_ALLOWED",
                       "invalidate에 GET")

    status, body, headers = send_request(address, "GET", "/stats")
    expected_names = {"size", "capacity", "hitCount", "missCount", "expiredCount"}
    runner.check(status == 200 and set(body.keys()) == expected_names, "통계 필드")
    runner.check(headers.get("Content-Type") == "application/json; charset=utf-8", "Content-Type")


test_basic_flow()
test_empty_value_and_missing_field()
test_body_validation()
test_ttl_validation()
test_duplicate_field()
test_ttl_endpoints()
test_expiry()
test_invalidate()
test_keys()
test_value_size()
test_payload_too_large()
test_routes_and_stats()
runner.finish()
