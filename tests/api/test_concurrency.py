"""협약 15.3 동시성 테스트.
실행 : python3 tests/api/test_concurrency.py [호스트:포트]   (서버를 먼저 띄운다)
"""

import threading
import time
from concurrent.futures import ThreadPoolExecutor

from api_client import TestRunner, key_path, read_server_address, send_request

WORKER_COUNT = 16
WRITE_COUNT = 1000
READ_COUNT = 300
SAME_KEY_REQUEST_COUNT = 200
RUN_ID = str(int(time.time() * 1000))

address = read_server_address()
runner = TestRunner("test_concurrency")


def make_key(name):
    return f"concurrency:{RUN_ID}:{name}"


def put_value(key, value):
    return send_request(address, "PUT", key_path(key), '{"value": "' + value + '"}')


def test_read_while_writing():
    """A를 반복 조회하는 동안 다른 키를 많이 저장한다. rehash가 일어나도 A와 모든 저장이 남아야 한다"""
    key_a = make_key("A")
    put_value(key_a, "keep")
    is_writing = threading.Event()
    is_writing.set()
    read_results = []

    def read_a_repeatedly():
        while is_writing.is_set() and len(read_results) < READ_COUNT * 10:
            read_results.append(send_request(address, "GET", key_path(key_a))[:2])

    reader = threading.Thread(target=read_a_repeatedly)
    reader.start()
    with ThreadPoolExecutor(WORKER_COUNT) as pool:
        write_results = list(pool.map(lambda i: put_value(make_key(f"w{i}"), str(i))[0],
                                      range(WRITE_COUNT)))
    is_writing.clear()
    reader.join()

    runner.check(all(result == (200, {"value": "keep"}) for result in read_results),
                 "동시 저장 중 A 값 유지")
    runner.check(all(status == 200 for status in write_results), "모든 저장 성공")
    with ThreadPoolExecutor(WORKER_COUNT) as pool:
        values = list(pool.map(lambda i: send_request(address, "GET",
                                                      key_path(make_key(f"w{i}")))[1],
                               range(WRITE_COUNT)))
    runner.check(all(values[i] == {"value": str(i)} for i in range(WRITE_COUNT)),
                 "동시 저장이 끝난 뒤 모두 조회")


def test_delete_same_key():
    key = make_key("delete")
    put_value(key, "x")
    with ThreadPoolExecutor(WORKER_COUNT) as pool:
        results = list(pool.map(lambda _: send_request(address, "DELETE", key_path(key))[1],
                                range(SAME_KEY_REQUEST_COUNT)))
    deleted_count = sum(1 for body in results if body == {"deleted": True})
    runner.check(deleted_count == 1, f"같은 키 동시 삭제에서 deleted: true는 한 번 ({deleted_count})")


def test_put_same_key():
    key = make_key("put")
    values = [f"value-{i}" * 50 for i in range(SAME_KEY_REQUEST_COUNT)]
    with ThreadPoolExecutor(WORKER_COUNT) as pool:
        list(pool.map(lambda value: put_value(key, value), values))
    final_value = send_request(address, "GET", key_path(key))[1]["value"]
    runner.check(final_value in values, "같은 키 동시 저장 뒤 그중 하나가 온전히 남음")


def test_put_and_delete_mixed():
    key = make_key("mixed")

    def put_or_delete(number):
        if number % 2 == 0:
            return put_value(key, "x")[0]
        return send_request(address, "DELETE", key_path(key))[0]

    with ThreadPoolExecutor(WORKER_COUNT) as pool:
        statuses = list(pool.map(put_or_delete, range(SAME_KEY_REQUEST_COUNT)))
    runner.check(all(status == 200 for status in statuses), "PUT·DELETE 섞어도 모두 응답")
    final_status, final_body, _ = send_request(address, "GET", key_path(key))
    is_consistent = (final_status, final_body) == (200, {"value": "x"}) or final_status == 404
    runner.check(is_consistent, "마지막 상태가 일관됨")


test_read_while_writing()
test_delete_same_key()
test_put_same_key()
test_put_and_delete_mixed()
runner.finish()
