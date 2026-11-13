# 미니 레디스

직접 만든 해시 테이블로 키-값 저장소를 만들고, HTTP API로 외부에서 쓸 수 있게 한 프로젝트다.
작업 규칙은 모두 [`docs/미니레디스_협약.md`](docs/미니레디스_협약.md)를 따른다. 협약이 코드보다 우선한다.

## 빌드·실행·테스트

```bash
make              # 미니 레디스 서버와 샘플 API 빌드 (build/ 폴더)
make test         # 단위 테스트 실행 (store, hash_table, json, http)
make test-asan    # 단위 테스트를 AddressSanitizer로 실행
make test-api     # 서버를 띄워 외부 기능·동시성 테스트 (tests/api)
make test-demo    # 캐시 샘플 API 테스트 (tests/demo)
make test-all     # 위 네 가지 모두
make bench        # 캐시 ON/OFF 성능 비교 (bench/results/bench_result.json)
make bench-store  # 해시 테이블 저장·조회 100만 회
make run          # 미니 레디스 서버 실행 (기본 포트 6380)
make run-demo     # 캐시 샘플 API 실행 (기본 포트 3000)
make clean        # 빌드 결과 삭제
```

## 포트

| 서버 | 기본 포트 |
|---|---|
| 미니 레디스 | 6380 |
| 샘플 API | 3000 |

## 폴더 구성

| 위치 | 내용 |
|---|---|
| `src/store/` | Phase 1 엔진 (해시 테이블, store, 단조 시계) |
| `src/server/` | Phase 2 HTTP API 서버 |
| `src/client/` | 미니 레디스 HTTP 클라이언트 (샘플 API가 사용) |
| `src/demo/` | Phase 2 캐시 샘플 API |
| `tests/` | 단위 테스트, `tests/api/`는 세 서버 공통 기능 테스트 |
| `bench/` | 성능 비교 스크립트와 원본 결과 |
| `docs/` | 협약, 구현 안내, 성능 비교 결과 |
| `ai/` | AI 진입 파일 (CLAUDE.md, GEMINI.md, AGENTS.md) |

바꾸거나 새로 만든 주요 파일은 이 표에 위치를 적는다. (협약 5장)

### 협약 5장 트리에 없던 파일

| 위치 | 내용 |
|---|---|
| `src/server/json.h / .c` | 요청 JSON 읽기(중복 필드·정수 판정)와 응답 JSON 쓰기. demo·client도 같이 쓴다 |
| `src/server/tcp_server.h / .c` | 연결 받기와 워커 스레드 풀, SIGINT·SIGTERM 종료. demo도 같이 쓴다 |
| `src/client/mini_redis_client.h / .c` | 미니 레디스 HTTP 클라이언트 (협약 11.6 선택 모듈) |
| `tests/store/test_hash_table.c` | 해시 테이블 전용 테스트 (같은 칸 충돌, rehash 경계) |
| `tests/server/test_json.c`, `test_http.c` | JSON·HTTP 단위 테스트 |
| `tests/api/test_api.py` | 공통 기능·동시성 테스트. `python3 tests/api/test_api.py http://호스트:포트` |
| `tests/demo/test_demo.py` | 캐시 샘플 API 테스트 (두 서버를 직접 띄우고 끈다) |
| `bench/run_bench.py` | 캐시 ON/OFF 성능 비교 |
| `bench/bench_store.c` | 해시 테이블 100만 회 측정 (store를 직접 링크하는 예외) |
| `bench/results/bench_result.json` | 성능 비교 원본 결과 |

## 사용한 외부 라이브러리

- 없음. HTTP·JSON 파싱과 HTTP 클라이언트를 직접 구현했다. 테스트·벤치는 Python 3 표준 라이브러리만 쓴다.
