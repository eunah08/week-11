# 구현 안내

> 작성자 : sk (통합본 : EA·SK·JK 세 구현을 합침)
> 협약 17.1 항목을 채운다. 16장 질문에 AI 없이 답할 수 있을 때까지 다듬는다.

## 통합 기록

세 구현(EA·SK·JK)을 협약 15장 방식으로 비교했다.
세 사람의 테스트를 서로의 구현에 모두 돌려 보고, 통과하지 않는 곳을 규칙이 어긋난 곳으로 봤다.

### 교차 테스트 결과

store 단위 테스트 (`store.h`가 셋 다 같아서 그대로 바꿔 끼움, ASan + UBSan) :

| 구현 \ 테스트 | EA 테스트 | SK 테스트 | JK 테스트 |
|---|---|---|---|
| EA store | 통과 | 5개 실패 | 1개 실패 |
| SK store | 통과 | 통과 | 통과 |
| JK store | 통과 | 통과 | 통과 |

- EA store는 UTF-8이 아닌 키(`\xff`, 잘린 바이트 열)를 받아들였다. 협약 7.1은 키를 UTF-8 문자열로 정한다.

외부 API 테스트 (서버를 띄우고 주소만 바꿔 실행) :

| 서버 \ 테스트 | EA 기능·동시성 | SK | JK |
|---|---|---|---|
| EA 서버 | 통과 | 실패 | 실패 |
| SK 서버 | 통과 | 통과 | 통과 |
| JK 서버 | 통과 | 실패 | 통과 |

- EA 서버 : 정밀 소수 `ttlMs`(`1.00000000000000000001`, `2147483647.00000001`)를 정수로 받아들였다.
  UTF-8이 아닌 키(`%ff`)를 받아들였다.
  2MiB를 넘는 본문에 `413`을 보내기 전에 연결이 끊겼다 (협약 11.5 "읽기를 멈추고 413을 돌려준다").
- JK 서버 : 2MiB를 넘는 본문에 `413`을 보내기 전에 연결이 끊겼다.
- 두 서버 모두 선택 엔드포인트 `/exists`, `/health`가 없다.
  SK 테스트의 연결 끊김·느린 연결·잘못된 요청 줄 검사는 끝에 `/health`로 서버가 살아 있는지 보기 때문에 함께 실패했다.
  이 부분은 서버가 죽은 것이 아니다.

### 본체로 SK 구현을 고른 이유

- 세 사람의 단위 테스트와 API 테스트를 모두 통과한 유일한 구현이다.
- `hash_table`이 시간과 만료를 모른다 (협약 6장). EA·JK는 해시 테이블 노드에 `expires_at_ms`가 있다.
- JK의 할당 실패 주입 테스트(`test_allocation.c`)도 SK store에서 그대로 통과했다.
- 구현 안내(이 문서)와 성능 비교가 협약 17.1·15.4 항목을 가장 많이 채웠다.
  첫 요청 분리, 동시 요청 수 변화, 해시 테이블 100만 회 측정이 있다.

### 다른 구현에서 가져온 것

| 가져온 것 | 출처 | 위치 |
|---|---|---|
| 공개 함수 경계값·잘못된 입력·만료 항목 1621검사 | JK | `tests/store/test_store_edge_cases.c` |
| 할당 실패 주입 테스트와 Makefile 규칙 | JK | `tests/store/test_allocation.c`, `fail_alloc.h` |
| 주기적 정리 스레드 테스트 (SK의 `sweeper_start(store, mutex)`에 맞게 옮김) | JK | `tests/server/test_sweeper.c` |
| 외부 API 경계값·분할 요청·정리 중 재저장 테스트 | JK | `tests/api/test_api_edge.py` |
| README의 `curl` 사용 예 (미니 레디스 부분) | JK | `README.md` |
| store 기본 시나리오 단위 테스트 | EA | `tests/store/test_store_basic.c` |
| 외부 API 기본 흐름·동시성 테스트와 HTTP 도우미 | EA | `tests/api/test_api_basic.py`, `test_concurrency.py`, `api_client.py` |
| `.gitignore`의 `__pycache__/` | EA | `.gitignore` |

- `make test`와 `make test-api`가 위 테스트를 모두 함께 돌린다.

### 가져오지 않은 것

- EA·JK의 `src/` 코드 : 위 교차 테스트에서 통과하지 못한 곳이 있고, 협약 6장 경계가 달라서 본체와 섞지 않았다.
- EA·JK의 캐시 데모 테스트 : 상품 응답 모양이 구현마다 달라(EA는 `id`가 문자열, JK는 `{"value"}`) SK 샘플 API에는 맞지 않는다.
- EA·JK의 성능 측정 결과 : 다른 구현을 잰 값이라 섞지 않았다. `benchmark.md`는 SK 구현의 측정값이다.

### 팀이 정할 것

- `PUT /products/{id}`에서 캐시 삭제가 실패할 때 : SK·EA는 `200`, JK는 `503 CACHE_UNAVAILABLE`을 돌려줬다.
  통합본은 SK 방식(아래 "협약에 없어서 정한 것" 참고)을 따른다.

## 빌드·실행·테스트 명령

WSL(Ubuntu) 또는 Linux·macOS에서 실행한다. 외부 라이브러리는 없고 `gcc`(또는 `cc`), `make`, `python3`만 있으면 된다.

```bash
make              # build/mini_redis, build/sample_api
make test         # 단위 테스트 (store, hash_table, 할당 실패, json, http, sweeper)
make test-asan    # 같은 단위 테스트를 AddressSanitizer + UBSan으로
make test-api     # 서버를 16380 포트에 띄워 tests/api/의 테스트 4묶음 실행 후 SIGTERM으로 종료 확인
make test-demo    # 미니 레디스 + 샘플 API를 띄워 tests/demo/test_demo.py 실행
make test-all     # 위 네 가지
make bench        # 캐시 ON/OFF 성능 비교 → bench/results/bench_result.json
make bench-store  # 해시 테이블 저장·조회 100만 회
make run          # 미니 레디스 (포트 6380, ./build/mini_redis [포트])
make run-demo     # 샘플 API (포트 3000, ./build/sample_api [포트] [미니 레디스 포트] [CACHE_TTL_MS])
```

공통 기능 테스트는 서버 주소만 바꿔 다른 팀원 서버에도 돌릴 수 있다.

```bash
python3 tests/api/test_api.py http://127.0.0.1:6380
```

## 해시 함수와 충돌 처리 방식, 고른 이유

- **해시 함수 : FNV-1a 64비트** (`hash_table_hash_key`)
  - 바이트마다 `hash ^= byte; hash *= FNV_PRIME`. 코드가 4줄이라 설명하기 쉽고, 짧은 문자열 키에서도 비트가 고르게 섞인다.
  - XOR을 먼저 하고 곱하는(1a) 순서라서 마지막 바이트까지 아래 비트에 고르게 퍼진다. 칸 번호를 아래 비트로 정하는 방식과 잘 맞는다.
- **칸 번호 : `hash & (capacity - 1)`**
  - 칸 수를 항상 2의 거듭제곱으로 둔다. 그러면 나머지 연산(`%`) 대신 아래 비트만 남기면 된다.
  - `store_options_t.initial_capacity`가 2의 거듭제곱이 아니면 위로 올린다(10 → 16).
- **충돌 처리 : 체이닝 (칸마다 단일 연결 리스트)**
  - 새 항목은 리스트 맨 앞에 붙인다(O(1)).
  - 노드에 해시값을 같이 저장한다. 그래서 찾을 때는 해시가 같을 때만 `strcmp`를 하고, rehash 때는 해시를 다시 계산하지 않는다.
  - 개방 주소법과 달리 삭제할 때 삭제 표시(tombstone)가 필요 없다. 앞·중간·끝 노드를 "이 노드를 가리키는 포인터의 주소(`entry_t **link`)" 하나로 똑같이 떼어 낸다.
- **경계 (협약 6장)**
  - `hash_table`은 시간과 만료를 모른다. 노드에는 `key`와 `void *item`만 있다.
  - 값과 만료 시각 묶음(`store_item_t`)은 store가 정하고, 테이블은 항목을 뺄 때 생성 시 받은 `destroy_item`으로 해제만 한다.

## rehash 기준과 방식

- **기준** : 넣은 뒤 `size * 4 > capacity * 3`(적재율 3/4 초과)이면 칸을 **2배**로 늘린다. 실수 비교를 피하려고 정수 곱셈으로 계산한다.
- **방식** : 새 칸 배열을 `calloc`하고, 모든 노드를 새 칸 번호(`hash & (new_capacity - 1)`)로 옮긴다.
  - 노드는 새로 만들지 않고 포인터만 옮긴다.
  - 칸 수가 바뀌면 같은 해시라도 칸 번호가 바뀌므로 전부 다시 걸어야 한다.
- **한 번에(stop-the-world) 한다** : 그 순간 O(n) 지연이 생긴다. 100만 개를 넣는 동안 rehash가 17번 일어나며, 그 비용이 아래 "저장 (새 키)" 시간에 들어 있다.
- 새 칸 배열을 받지 못하면(메모리 부족) 그대로 둔다. 삽입 자체는 성공이다.
- 칸 수를 줄이지는 않는다 (알려진 제한).

## 주요 함수의 동작과 시간 복잡도

| 함수 | 동작 | 평균 | 최악 |
|---|---|---|---|
| `store_set` | 검증 → 값 복사 → 살아 있는 항목이면 값·만료 교체, 아니면 새로 삽입 → 필요하면 rehash | O(1) | O(n) (rehash, 한 칸 몰림) |
| `store_get` | 검증 → 찾기(만료면 지우고 없음) → 값 복사본 | O(1) + 값 길이 | O(n) |
| `store_delete` / `store_exists` / `store_expire` / `store_ttl` | 검증 → 살아 있는 항목 찾기 → 처리 | O(1) | O(n) |
| `store_delete_by_prefix` | 칸 전체를 돌며 접두사가 같으면 지운다 | O(capacity + n) | 같음 |
| `store_remove_expired` | 지난번 칸부터 한 바퀴, 만료 항목을 최대 `max_count`개 | O(capacity + n) | 같음 |
| `store_size` / `store_get_stats` | 만료 항목을 모두 정리한 뒤 개수 | O(capacity + n) | 같음 |

- **set 한 번의 흐름** : `validate_set_input` → `copy_text(value)` → `find_live_item` → 있으면 `free(옛 값)` 후 교체, 없으면 `insert_new_item` → `hash_table_insert` → 적재율 확인 → `grow_buckets`
- **get 한 번의 흐름** : `is_valid_key` → `find_live_item`(해시 → 칸 → 리스트에서 해시·문자열 비교 → 만료면 `hash_table_remove`) → 값 복사본을 돌려준다(호출한 쪽이 `free`)
- 최악 O(n)은 모든 키가 한 칸에 몰렸을 때다. FNV-1a는 암호학적 해시가 아니라서, 일부러 충돌을 만드는 입력(hash flooding)을 막지 못한다 (알려진 제한).

## TTL 만료 처리 방식 (지연 삭제, 주기적 정리의 대상 선택 방식)

- 만료 시각은 `expires_at_ms = now_ms() + ttl_ms`이다. 단조 시계를 쓰고, 만료가 없으면 `NO_EXPIRY`(0)이다.
  - `expires_at_ms <= now_ms()`이면 만료로 본다(정각 포함).
  - 단조 시계를 쓰는 이유 : 시스템 시각을 바꿔도 거꾸로 가지 않아 만료 판정이 흔들리지 않는다. 테스트에서는 `store_options_t.now_ms`에 가짜 시계를 끼워 기다리지 않고 검증한다.
- **지연 삭제** : 키를 읽는 공개 함수는 모두 `find_live_item`을 거친다. 만료된 항목을 만나면 그 자리에서 지우고 `expired_count`를 올린다. 그래서 만료 항목은 모든 공개 함수에서 "없는 키"로 보인다.
- **주기적 정리** : `sweeper` 스레드가 1000ms마다 store 잠금을 잡고 `store_remove_expired(200)`을 부른다. 기다리는 조건 변수도 `CLOCK_MONOTONIC`으로 만들어서, 시스템 시각을 바꿔도 정리 주기가 흔들리지 않는다.
- `store_ttl`은 시계를 한 번만 읽어 만료 판정과 남은 시간 계산에 같이 쓴다. 두 번 읽으면 그 사이에 만료된 키가 `-1`(만료 없음)로 보일 수 있다.
  - **대상 선택** : store가 커서(`sweep_bucket`)를 기억한다. 지난번에 멈춘 칸부터 이어서 최대 한 바퀴 돈다.
  - 한 번에 지우는 개수에 상한이 있어서 다른 요청을 오래 막지 않는다. 다만 순회하는 칸 수는 최대 `capacity`다.
- **두 가지로 나누는 이유** : 지연 삭제만 쓰면 아무도 다시 읽지 않는 만료 키가 메모리에 계속 남는다. 주기적 정리만 쓰면 정리 주기 사이에 만료 키가 보일 수 있다.
- **판정과 삭제 사이의 경쟁** : sweeper도 요청 처리와 같은 mutex 안에서 `store_remove_expired` 한 번으로 판정과 삭제를 끝낸다. 그래서 그 사이에 같은 키가 다시 저장될 틈이 없다(`test_resave_before_sweep_keeps_new_value`).
- `expired_count`는 지연 삭제, 주기적 정리, 접두사 삭제 중 만료된 것, `store_size`가 정리한 것을 모두 센다.

## 무효화 방식과 고른 이유

| 방식 | API | 언제 쓰나 |
|---|---|---|
| 키 하나 바로 삭제 | `DELETE /keys/{key}` | 원본이 바뀐 것을 정확히 알 때 (샘플 API의 `PUT /products/{id}`) |
| TTL 단축·재설정 | `PUT /keys/{key}/ttl`, `PUT /keys/{key}`의 `ttlMs` | 조금 늦게 반영돼도 될 때. 무효화를 깜빡해도 언젠가 사라진다 (샘플 API는 모든 캐시에 60초) |
| 접두사 일괄 삭제 | `POST /invalidate` | 관련 키 묶음을 한 번에 버릴 때 (`product:` 전체 등). 테이블 전체를 훑으므로 O(n) |

- 키 하나 삭제와 TTL은 필수 조합이다. 정확히 아는 변경은 바로 지우고, 놓친 변경은 TTL로 결국 사라지게 한다.
- 빈 접두사는 거부한다. 전체 삭제는 `flush`(선택)로만 한다. 이번 구현에서는 만들지 않았다.

## 동시성 구조와 고른 이유

**워커 스레드 풀 8개 + store 전체를 잠그는 `pthread_mutex` 하나** (`src/server/tcp_server.c`, `handlers.c`)

```text
메인 스레드 : poll → accept → 연결 큐(최대 1024)
워커 8개    : 큐에서 꺼냄 → 요청 읽기 → [잠금] store 함수 한 번 [해제] → 응답 쓰기 → 닫기
sweeper     : 1초마다 [잠금] store_remove_expired(200) [해제]
```

- **12.1을 지키는 방법** : 모든 store 호출이 같은 mutex 안에서 한 번에 끝난다. 그래서 명령 하나는 한 시점에 적용된 것처럼 보인다.
  - 값이 반쯤 쓰인 상태를 읽을 수 없다.
  - 같은 키를 동시에 삭제하면 먼저 잠근 쪽만 `true`다.
  - rehash 중에는 다른 요청이 잠금을 기다린다.
- **잠금 안에서 하지 않는 것** : 네트워크 읽기·쓰기, JSON 해석과 응답 만들기. 본문 검증은 잠그기 전에 끝내고, 응답 JSON은 잠금을 푼 뒤 만든다.
- 잠금이 하나뿐이라 순서 문제(교착 상태)가 없다. 다만 sweeper 내부의 `state_mutex`는 store 잠금과 동시에 쥐지 않는다.
- **고른 이유** : 해시 테이블과 rehash를 그대로 두고 정확성을 가장 쉽게 설명할 수 있다.
  - 칸별 잠금은 rehash와 함께 쓰기 어렵다.
  - 읽기·쓰기 잠금은 `get`도 지연 삭제와 통계 때문에 쓰기가 되므로 이득이 작다.
  - store 호출 자체는 수백 ns라서, 요청 시간 대부분(소켓 처리)은 잠금 밖에서 병렬로 돈다.
- **한계** : store 연산은 한 번에 하나뿐이다. `store_delete_by_prefix`·`store_size`(O(n))·rehash가 도는 동안 다른 요청이 모두 기다린다.
- **종료** : SIGINT·SIGTERM을 받으면 순서대로 정리한다.
  1. accept를 멈춘다.
  2. 이미 받은 연결은 처리한 뒤 워커를 join한다.
  3. sweeper를 깨워 join한다.
  4. store를 해제한다.
  - `SIGPIPE`는 무시하고, 쓰기에는 `MSG_NOSIGNAL`을 쓴다.

## 사용한 외부 라이브러리

- 없음. HTTP 파싱(`src/server/http.c`), JSON 파싱·출력(`src/server/json.c`), HTTP 클라이언트(`src/client/mini_redis_client.c`)를 모두 직접 구현했다.
- 테스트와 벤치는 Python 3 표준 라이브러리만 쓴다.

## 영속성 방식 비교

이번 구현에서는 영속성을 구현하지 않았다 (Optional). 서버를 끄면 데이터가 사라진다.

| 방식 | 장점 | 단점 | 이 프로젝트에 적합한가 |
|---|---|---|---|
| 스냅샷 (RDB) | 파일이 작고 복구가 빠르다. 쓰기 경로에 비용이 없다 | 마지막 저장 이후 데이터를 잃는다. 저장하는 동안 전체를 훑어야 한다(전체 잠금 구조에서는 그동안 요청이 멈춤) | 캐시 용도라면 충분하다. 잃어도 원본에서 다시 채운다 |
| 명령 기록 (AOF) | 잃는 데이터가 적다(매번 fsync면 거의 없음) | 파일이 계속 커진다. 복구가 느리다. 쓰기마다 디스크 I/O가 생긴다 | 데이터 자체가 중요한 저장소라면 이쪽. 지금 구조에서는 잠금 안에서 기록해야 순서가 보장된다 |
| 둘 다 | 빠른 복구 + 적은 손실 | 구현이 가장 복잡하다 | 과하다 |

**결론** : 이 프로젝트의 주 용도는 샘플 API의 캐시(원본이 따로 있음)라서, 데이터를 잃어도 원본에서 다시 가져오면 된다. 그래서 구현한다면 **주기적 스냅샷**이 적합하다. "최대한 안전하게"가 목표라면 1초마다 fsync하는 AOF를 고른다(최대 1초 손실, 쓰기 지연은 작음).

## 성능 비교 요약 (자세한 결과는 benchmark.md)

- 원본이 300ms 걸리는 상품 조회에서 캐시 ON은 적중 요청을 수 ms 안에 돌려준다. 같은 상품을 반복 조회하면 원본 조회가 200번에서 1번으로 줄어든다.
- 모든 요청이 새 상품이면 적중률이 0%다. 캐시 조회와 저장(HTTP 왕복 2번)만큼 느려지지만, 300ms에 비하면 무시할 만하다.
- 해시 테이블 자체 : 100만 개 기준 조회 1회 약 0.25~0.37µs, 새 키 저장 1회 약 0.63~0.93µs(rehash 포함, 실행마다 흔들림).

## 알려진 제한

- `store_delete_by_prefix`는 테이블 전체를 훑으므로 O(n)이다.
- `store_size`·`store_get_stats`(`GET /stats`)는 정확한 개수를 위해 만료 항목을 모두 정리하므로 O(n)이다.
- rehash를 한 번에 하므로 그 순간 O(n) 지연이 있다. 칸 수를 줄이지 않는다.
- 전체 잠금이라 store 연산이 동시에 진행되지 않는다.
- keep-alive를 지원하지 않는다. 요청마다 연결을 맺고 닫는다.
- chunked 전송을 지원하지 않는다(`400 INVALID_BODY`). 헤더는 16KiB까지 받는다.
- 요청 하나(헤더 + 본문)를 10초(`HTTP_REQUEST_TIMEOUT_MS`) 안에 다 받지 못하면 응답 없이 연결을 닫는다. 그래서 느리게 보내는 연결이 워커를 붙잡는 시간도 최대 10초다. 다만 워커 8개를 모두 붙잡으면 그 10초 동안은 다른 요청이 기다린다.
- 요청 본문의 최상위 필드는 64개(`JSON_MAX_FIELDS`)까지만 받는다. 넘으면 `400 INVALID_BODY`다. 중복 확인이 O(필드 수²)라서, 큰 본문 하나로 CPU를 붙잡지 못하게 막는 상한이다.
- 연결 큐(1024)가 가득 차면 새 연결을 바로 닫는다.
- 해시 함수가 비암호학적이라 의도적인 충돌 공격에 약하다.
- 같은 빗나감 키로 요청이 몰리면 원본이 그만큼 불린다 (stampede 방지 없음. 테스트에서 동시 10개 → 원본 10번).
- 영속성이 없다.

## 추가 구현 요소

- 선택 엔드포인트 `GET /keys/{key}/exists`, `GET /health`
- 키는 UTF-8 검사를 한다 (협약 7.1 "UTF-8 문자열"). 잘못된 바이트 열은 `INVALID_KEY`다.
- JSON 정수 판정을 원문 자릿수로 한다. `2147483647.00000001`처럼 double로 바꾸면 정수가 되는 값도 거부한다.

## 협약에 없어서 정한 것 (협약 반영 후보)

| 항목 | 정한 내용 |
|---|---|
| `hash_table` 공개 함수 | `hash_table_create(capacity, destroy_item)`, `hash_table_find`, `hash_table_insert`, `hash_table_remove`, `hash_table_remove_matching`, `hash_table_hash_key` |
| `store_get`의 `out_value`가 NULL | `STORE_ERR_INVALID_VALUE` |
| `expired_count`에 세는 것 | 지연 삭제 + 주기적 정리 + 접두사 삭제 중 만료 + `store_size` 정리 |
| 서버 내부 모듈 | `json.*`(JSON), `tcp_server.*`(연결 받기 + 워커 풀) 새로 만듦 |
| `sweeper_start` 시그니처 | `sweeper_start(store_t *store, pthread_mutex_t *store_mutex)` |
| HTTP 형식 오류(요청 줄·헤더) | `400 INVALID_BODY` |
| 404·405 에러 메시지의 함수 이름 | `[router_handle_request]`, HTTP 읽기 실패는 `[http_read_request]` |
| 샘플 API 실행 인자 | `sample_api [포트] [미니 레디스 포트] [CACHE_TTL_MS]` |
| 상품 원본 | id 1~1,000,000은 있음. `{"id","name","price","version"}`, `PUT` 본문 `{"name": 문자열, "price": 0 이상 정수}` |
| 샘플 API 에러 code | `PRODUCT_NOT_FOUND`(404), `INVALID_PRODUCT_ID`(400) 추가 |
| `?cache=off` 응답 | `X-Cache` 헤더를 붙이지 않는다 |
| 미니 레디스가 꺼졌을 때 | 1초 시간 초과 뒤 원본으로 응답, `X-Cache: MISS` |
| `PUT /products/{id}`에서 캐시 삭제가 실패할 때 | 원본은 이미 바뀌었으므로 `200`을 돌려주고 stderr에 남긴다. 협약 13.3("둘 다 끝낸 뒤 성공")과 13.2("캐시 장애가 서비스 장애가 되지 않게")가 부딪히는 경우라 13.2를 따랐다. 그동안 옛 값은 TTL(60초)이 지나면 사라진다 |
| 요청 전체 마감 | 10초 (`HTTP_REQUEST_TIMEOUT_MS`), 닫기 전 남은 본문 버리기 1초 |
| JSON 필드 수 상한 | 최상위 64개 (`JSON_MAX_FIELDS`), 넘으면 `INVALID_BODY` |
| 퍼센트 디코딩 메모리 부족 | `500 INTERNAL_ERROR` (형식 오류만 `400 INVALID_KEY`) |

## 핵심 로직을 설명할 때 참고할 파일

| 주제 | 파일 · 함수 |
|---|---|
| 해시·칸 번호·충돌·rehash | `src/store/hash_table.c` : `hash_table_hash_key`, `find_bucket_index`, `hash_table_insert`, `grow_buckets`, `unlink_entry` |
| 만료·지연 삭제 | `src/store/store.c` : `find_live_item`, `is_item_expired`, `store_remove_expired` |
| 무효화 | `src/store/store.c` : `store_delete`, `store_expire`, `store_delete_by_prefix`(`is_prefix_match`) |
| 메모리 소유권 | `store_set`(키·값 복사), `store_get`(복사본 반환), `insert_new_item`(실패 시 소유권 안 넘어감) |
| 동시성 | `src/server/handlers.c` : `lock_store`·`unlock_store` 위치, `src/server/tcp_server.c`, `src/server/sweeper.c` |
| 요청 → 해시 테이블 경로 | `tcp_server.c run_worker` → `main.c handle_connection` → `http_read_request` → `router_handle_request` → `handlers_put_key` → `store_set` → `hash_table_insert` |
| Cache-Aside | `src/demo/sample_api.c` : `handle_get_product`, `respond_from_origin`, `handle_put_product` |
