# 미니 레디스

C11로 직접 구현한 FNV-1a 체이닝 해시 테이블과 HTTP 키-값 저장소다.
Phase 1 엔진과 Phase 2 서버·캐시 데모를 구현했다.
공개 계약은 [팀 협약](docs/미니레디스_협약.md) 8·11장이다.

## 빌드와 실행

POSIX Linux/macOS, C11 컴파일러, Make, Python 3(테스트/벤치마크), curl(수동 확인)이 필요하다.
외부 라이브러리는 사용하지 않는다. POSIX 소켓·pthread와 C 표준 라이브러리만 링크한다.

```sh
make
make run                     # 127.0.0.1:6380
# 다른 터미널
make run-demo                # 127.0.0.1:3000, 캐시 6380
```

포트와 데모 조건은 실행 인자로 바꾼다.

```sh
./build/mini_redis 16380
./build/sample_api 13000 16380 60000 300
# sample_api [port [redis_port [ttl_ms [origin_delay_ms]]]]
```

기본 TTL은 60000ms, 원본 지연은 300ms다. 서버는 루프백에 바인딩하며
SIGINT/SIGTERM으로 종료한다. 요청마다 연결을 닫는다. 본문 상한 2MiB,
저장 값 상한 1MiB, 키 상한 256바이트다. 키는 URL 인코딩해서 전달한다.
헤더+본문 수신에는 총 2초 제한이 있다. chunked 전송은 지원하지 않는다.

## 테스트와 벤치마크

```sh
make test                    # store 1621개 검사 + 할당 실패/JSON/sweeper/HTTP OOM
make test-http               # 서버 자동 실행·종료, 7개 API 검사와 데모 통합 검사
make test-asan               # ASan + UBSan 빌드와 전체 C 단위 테스트
ASAN_OPTIONS=detect_leaks=0 MINI_REDIS_BUILD_DIR=build/asan python3 tests/api/run_local.py
make bench                   # 기본: 60요청, 동시성4, 3회, 원본300ms, TTL60000ms
# 직접 조건 지정
python3 bench/compare.py --requests 60 --concurrency 4 --rounds 3 --delay-ms 300 --ttl-ms 60000
```

실행 중인 다른 구현에는 주소만 바꿔 공통 API 검사를 적용할 수 있다.

```sh
python3 tests/api/test_api.py http://127.0.0.1:6380
```

macOS ASan은 LeakSanitizer를 지원하지 않아 `detect_leaks=0`으로 실행한다.
별도 할당 실패 주입 테스트가 저장소의 모든 할당·해제 개수를 추적하고 마지막 0개를 확인한다.
의도적인 OOM 테스트의 `[router_dispatch] INTERNAL_ERROR` stderr는 예상 출력이다.
벤치마크는 매 모드·반복마다 두 프로세스를 새로 시작하며, 결과를
[docs/benchmark.md](docs/benchmark.md)와 [bench/results.json](bench/results.json)에 기록한다.

## HTTP 사용 예

```sh
curl -sS -X PUT 'http://127.0.0.1:6380/keys/product%3A42' \
  -H 'Content-Type: application/json' -d '{"value":"hello","ttlMs":5000}'
curl -sS 'http://127.0.0.1:6380/keys/product%3A42'
curl -sS -X PUT 'http://127.0.0.1:6380/keys/product%3A42/ttl' \
  -d '{"ttlMs":1000}'
curl -sS 'http://127.0.0.1:6380/keys/product%3A42/ttl'
curl -sS -X DELETE 'http://127.0.0.1:6380/keys/product%3A42'
curl -sS -X POST 'http://127.0.0.1:6380/invalidate' -d '{"prefix":"product:"}'
curl -sS 'http://127.0.0.1:6380/stats'
```

상품 예제는 `{"value":"문자열"}`을 반환한다. 초기 상품값은 ID 문자열이고,
`fail` ID는 원본 실패(503)를 재현한다. `PUT` 본문은 `{"value":"새 값"}`이다.

```sh
curl -si 'http://127.0.0.1:3000/products/42'           # MISS
curl -si 'http://127.0.0.1:3000/products/42'           # HIT
curl -sS -X PUT 'http://127.0.0.1:3000/products/42' -d '{"value":"new"}'
curl -si 'http://127.0.0.1:3000/products/42'           # MISS, new
curl -si 'http://127.0.0.1:3000/products/42?cache=off' # 원본 직접 조회
curl -sS 'http://127.0.0.1:3000/demo/stats'
```

캐시 장애 시 GET은 원본으로 응답한다. 상품 PUT은 원본 변경 뒤 캐시 삭제가
실패하면 503을 반환한다(원본 변경은 이미 반영됨). 캐시 삭제 완료 전 성공을 알리지 않는다.
갱신과 조회가 겹치는 일관성 보강, 캐시 몰림 방지와 영속성은 선택 기능으로 남겼다.

## 파일과 설명

| 위치 | 역할 |
|---|---|
| `src/store/hash_table.h/.c` | 직접 구현한 해시·충돌·삽입·삭제·확장 |
| `src/store/store.h/.c`, `clock.h/.c` | 공개 계약·TTL·무효화·통계·단조 시계 |
| `src/server/http.h/.c` | 공통 HTTP 서버/클라이언트·입출력 제한·종료 |
| `src/server/json.h/.c` | UTF-8·JSON 파싱·인코딩·중복 필드 판정 |
| `src/server/router.*`, `handlers.*`, `api_errors.*` | REST 경로·검증·오류 매핑·저장소 mutex |
| `src/server/main.c`, `sweeper.*` | 저장소 생명주기·주기적 만료 정리 |
| `src/client/cache_client.h/.c` | 데모의 HTTP 전용 캐시 접근 |
| `src/demo/sample_api.c`, `slow_source.*` | 상품 API·느린 원본과 원본 조회 횟수 |
| `tests/store/`, `tests/server/` | 엔진·할당 실패·JSON·주기 정리 단위 테스트 |
| `tests/api/test_api.py`, `run_local.py` | 공통 외부 API 검사·로컬 실행기·종료 검사 |
| `tests/demo/test_demo.py` | MISS/HIT·수정·TTL·실패·캐시 장애·1MiB 값 |
| `bench/compare.py`, `results.json` | 재현 가능한 ON/OFF 측정·요청별 원본 자료 |
| `docs/implementation_notes.md` | 실제 함수 흐름·잠금·복잡도·영속성 비교·한계 |
| `docs/benchmark.md` | 실측 조건과 결과 |
| `ai/AGENTS.md` | AI 작업 지침 |
