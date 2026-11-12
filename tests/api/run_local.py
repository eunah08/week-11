#!/usr/bin/env python3
"""서버 생명주기까지 관리하는 통합 테스트 실행기."""
import http.client
import os
import pathlib
import socket
import subprocess
import sys
import time
import threading

ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILD = ROOT / os.environ.get('MINI_REDIS_BUILD_DIR', 'build')


def main():
    with socket.socket() as connection:
        connection.bind(('127.0.0.1', 0))
        port = connection.getsockname()[1]
    server = subprocess.Popen([str(BUILD / 'mini_redis'), str(port)])
    try:
        deadline = time.monotonic() + 5
        while True:
            if server.poll() is not None:
                raise RuntimeError('mini_redis exited before ready')
            try:
                connection = http.client.HTTPConnection('127.0.0.1', port, timeout=1)
                connection.request('GET', '/stats')
                response = connection.getresponse()
                response.read()
                if response.status == 200:
                    break
            except OSError:
                if time.monotonic() > deadline:
                    raise
                time.sleep(.02)
            finally:
                connection.close()
        subprocess.run([sys.executable, str(ROOT / 'tests/api/test_api.py'),
                        f'http://127.0.0.1:{port}'], check=True)
        # 종료 중 미완성 연결과 정상 요청을 함께 둔다. 유휴 수신 제한시간 안에 종료.
        with socket.create_connection(('127.0.0.1', port)) as pending:
            pending.sendall(b'PUT /keys/pending HTTP/1.1\r\nHost: local\r\n'
                            b'Content-Length: 100\r\n\r\n')
            def trickle():
                for _ in range(20):
                    try:
                        pending.sendall(b' ')
                    except OSError:
                        return
                    time.sleep(.3)
            feeder = threading.Thread(target=trickle, daemon=True)
            feeder.start()
            time.sleep(.02)
            server.terminate()
            server.wait(timeout=5)
            feeder.join(timeout=1)
        assert server.returncode == 0, server.returncode
        subprocess.run([sys.executable, str(ROOT / 'tests/demo/test_demo.py')], check=True)
        print('HTTP API, concurrency, pending-client shutdown, demo: OK')
    finally:
        if server.poll() is None:
            server.terminate()
            server.wait(timeout=5)


if __name__ == '__main__':
    main()
