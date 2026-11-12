# 구현 안내 — JK

## 작업 범위와 검증 기록

설계 기준은 [팀 협약](미니레디스_협약.md) 7~17장이다. `feat/redis_JK`의
초기 작업 트리는 깨끗했고, 기존 코드는 디렉터리·시그니처·오류 매핑·시계만 있었다.
초기 `make test`는 생성/해제 한 건뿐이었다. 현재 브랜치에서 이어 구현했으며
브랜치 변경·커밋·푸시·병합은 하지 않았다. 사용자 지시에 따라 Phase 1 이후 같은 세션에서
Phase 2까지 진행했다(협약 3.2의 단계별 병합 흐름에 대한 이번 작업의 명시적 예외).

- [x] Phase 1 공개 계약 테스트 먼저 작성: 스텁에서 1,612검사 중 1,057실패 확인.
- [x] 해시 테이블 → store 순서로 파일별 구현. UTF-8 경계 회귀를 더해 1,621검사 통과.
- [x] 모든 필수 REST, JSON 검증, 서버 mutex, 주기적 만료 정리, 신호 종료.
- [x] HTTP 전용 캐시 데모: MISS/HIT·무효화·TTL·캐시 장애·원본 실패·1MiB 값.
- [x] ON/OFF 반복 실측, README 실행법, 함수 흐름과 영속성 비교.

`make`, `make test`, `make test-http`, `make test-asan`을 사용한다.
ASan 서버 통합 검사는 `ASAN_OPTIONS=detect_leaks=0 MINI_REDIS_BUILD_DIR=build/asan
python3 tests/api/run_local.py`로 실행한다. 환경과 수치는 [benchmark.md](benchmark.md),
재현법은 [README](../README.md)를 참고한다. 외부 의존 라이브러리는 없다.

## 해시 테이블과 소유권

`src/store/hash_table.c`의 `find_bucket_index`는 64비트 FNV-1a를 쓴다.
각 UTF-8 바이트를 hash에 XOR한 뒤 소수 1099511628211을 곱한다.
초기값은 14695981039346656037이며 unsigned 정수의 오버플로는 모듈러 연산이다.
복잡한 라이브러리 없이 설명하기 쉽고 짧은 키에 적합하다. 보안용 해시는 아니므로
공격자가 충돌을 의도적으로 만든 경우에는 최악 O(n)이 된다.

버킷 번호는 `hash % capacity`다. 기본 capacity는 16이고, options의 임의 양수도
지원하므로 비트 마스크 대신 나머지를 사용한다. 동일 버킷의 노드는 연결 리스트로
이어진다. `hash_table_find_entry`는 각 노드의 키 전체를 `strcmp`로 비교한다.
체이닝은 충돌 노드를 따로 할당하고 삭제 시 링크를 잇는다. 개방 주소법은 배열의
다른 빈자리를 탐색하며 삭제 표식과 탐색 경로 유지가 추가로 필요하다.

`hash_table_put`은 기존 키이면 새 값 복사에 성공한 후 값·만료 시각을 함께 교체한다.
새 키이면 키·값·노드·필요한 새 버킷 배열을 확보하고 나서 테이블을 변경한다.
`hash_table_resize`는 새 배열 할당 실패 시 기존 배열을 유지한다. 성공하면 모든
노드를 새 capacity의 나머지로 다시 연결하고 이전 배열만 해제한다. 키·값은 재복사하지
않는다. 새 항목 수가 capacity × 0.75를 초과하면 2배로 늘린다. capacity가 바뀌면
같은 해시도 버킷 번호가 달라지므로 전체 노드를 다시 배치해야 한다.

키·값은 저장소가 복사·소유한다. `store_get`은 별도 복사본을 반환하며 호출자가
`free`한다. 실패하면 출력은 NULL이다. 삭제·만료·destroy는 내부 복사본을 해제한다.
할당 실패는 `STORE_ERR_NO_MEMORY`, 서버에서는 매핑 표 `api_errors_from_store_status`를
거쳐 500이다. URL 디코딩 할당 실패도 errno=ENOMEM을 통해 500으로 구분한다.
`tests/store/test_allocation.c`는 create·삽입·확장·덮어쓰기·get의 할당 실패 지점을
하나씩 주입하고 기존 값·TTL과 capacity 보존, 마지막 할당 잔여 0개를 검사한다.

## product:42 요청이 움직이는 실제 함수

빈 서버(capacity=16)에 다음 요청을 보낸다고 하자.

```http
PUT /keys/product%3A42 HTTP/1.1
Content-Type: application/json
Content-Length: 28

{"value":"old","ttlMs":5000}
```

위 예시의 Content-Length는 본문 UTF-8 바이트 수에 맞춰 HTTP 클라이언트가 계산한다.
실제 curl 예제는 README에 있다.

1. `http_serve`가 연결을 받아 `serve_worker`를 실행한다. `http_read`가 헤더와 본문을
   수신한다. 본문 2MiB 상한과 절대 수신 제한시간 2초를 적용한다.
2. `router_dispatch` → `parse_route`가 경로의 원문 `/ttl` 접미사를 먼저 판단하고
   `http_decode_key`가 `%3A`를 `:`로 복원한다. 키 길이·UTF-8·잘못된 인코딩을 검사한다.
   `%2Fttl`은 키 내용이고, 원문 `/ttl`만 TTL 하위 경로다. `+`는 공백으로 바꾸지 않는다.
3. `handlers_dispatch` → `parse_body` → `json_parse_object` → `validate_fields`가
   최상위 객체, 허용 필드, 누락·중복, 값 문자열, TTL 범위를 모두 확인한다.
   필드 이름의 JSON 이스케이프를 푼 뒤 중복을 비교한다. `ttlMs:1.0`은 허용하고
   정밀 소수도 원문 자릿수로 확인해 정수가 아니면 거부한다. 검증 중 store 상태는 바뀌지 않는다.
4. `execute_store`가 `server_context_t.mutex`를 잠그고 `execute_value` → `store_set`을
   호출한다. `validate_set` 후 주입받은 `now_ms`(기본 `clock_now_ms`)를 한 번 읽는다.
5. `hash_table_find_entry` → `find_bucket_index`에서 `product:42`의 FNV-1a 해시는
   **9480494942937486960**, 처음 16칸에서 **0번 버킷**이다. 충돌하면 키 전체를 비교한다.
   `hash_table_put`은 키와 값을 복사하고 `expires_at_ms = now_ms + 5000`을 함께 저장한다.
6. 잠금을 푼 다음 `respond_result`가 `{"created":true}`를 만들고 `http_write`가
   200 JSON으로 전송한다. 네트워크 작업 중에는 저장소 mutex를 잡지 않는다.

`GET /keys/product%3A42`는 같은 경로를 거쳐 `execute_value` → `store_get` →
`find_live_entry` → `hash_table_find_entry`로 간다. 버킷 0에서 키를 찾은 뒤
`is_expired`가 `expires_at_ms != 0 && expires_at_ms <= now_ms`를 검사한다.
경계 직전에는 값 복사본과 hit를 반환한다. 정각·이후에는 `hash_table_remove_entry`로
지연 삭제하고 expired/miss를 각각 올려 `STORE_NOT_FOUND`를 반환한다.
`respond_result`가 이를 404 `KEY_NOT_FOUND`로 바꾼다. 살아 있으면 `respond_value` →
`json_quote`로 안전하게 직렬화하고 200 `{"value":"old"}`를 보낸 뒤 복사본을 해제한다.
리사이징 후에는 해시를 새 capacity로 나눠 현재 버킷을 찾는다.

## TTL·무효화·통계

`store_set`의 0은 만료 없음이며 기존 TTL도 제거한다. 양수는 값과 TTL을 함께 교체한다.
`store_expire`는 1~MAX_TTL_MS만 받아 현재 시점부터 TTL을 다시 정한다.
`store_ttl`은 부재/만료면 NOT_FOUND, 무기한이면 -1, 나머지는 남은 ms다.
모든 키 연산의 만료 판단은 `find_live_entry` 안에서 한다. `store_set`은 기존 만료 항목도
할당 실패 시 보존하도록 새 값 확보 후 교체하며, 성공할 때 expired와 created를 반영한다.

`store_delete`는 키 하나를 즉시 지운다. `store_expire`는 지정 시간 뒤 자동 무효화한다.
`store_delete_by_prefix`는 전체를 순회해 `product:` 같은 그룹만 지우므로 O(capacity+n)이다.
만료 항목도 정리하지만 deletedCount에는 넣지 않는다. 빈 접두사는 오류다.
`store_size`와 `store_get_stats`는 남은 만료 항목까지 정리해 정확한 살아 있는 개수를 준다.
통계는 정리 후 복사하므로 expiredCount도 같은 관찰 시점과 일치한다. hit/miss는 get만 갱신한다.

`sweeper_start`의 `sweep_loop`는 기본 1초마다 같은 mutex 아래
`store_remove_expired(store, 200)`를 호출한다. `hash_table_visit`가 전체 버킷을 순회하되
삭제 개수를 200개로 제한한다. 시각·만료 판단·실제 삭제가 모두 한 잠금 안에서 진행되므로
그 사이 다른 요청이 재저장한 새 값을 과거의 판단으로 삭제할 수 없다.
`tests/server/test_sweeper.c`는 실제 스케줄링과 200개 물리 삭제를 관찰한다.
`sweeper_stop`은 플래그를 내린 뒤 join하며 저장소는 그 이후에 해제한다.

## 동시성과 종료

여러 요청은 최대 64개 작업 스레드로 처리하고 저장소 전체에 mutex 하나를 쓴다.
`execute_store`의 공개 store 호출 전체와 sweeper의 정리 전체가 같은 mutex를 공유한다.
get도 지연 삭제와 hit/miss 변경이 있으므로 쓰기와 같은 잠금이 필요하다.
입력 파싱·검증·JSON 응답 인코딩·네트워크 수신/전송은 밖에서 한다.
get의 복사본도 잠금 안에서 확보하므로 잠금을 푼 뒤 다른 요청이 삭제해도 안전하다.

리사이징과 다른 키 저장도 같은 경계 안에 있어 조회가 잠시 기다릴 수 있지만
반쯤 바뀐 버킷을 읽거나 값을 잃지 않는다. 동시 DELETE는 첫 명령만 true다.
하나의 store 잠금만 있어 store 관련 교착 순환이 없다. HTTP 작업 수 관리의 mutex는
store를 잡은 상태에서 취하지 않는다. 데모의 원본 mutex는 네트워크나 지연 대기 중 잡지 않는다.

SIGPIPE는 무시하고 SIGINT/SIGTERM은 수락 루프를 멈춘다. 이미 수락한 작업을 기다린 다음
sweeper를 멈추고 store를 해제한다. 수신은 단조 시계의 절대 제한시간을 사용해,
연결이 0.3초마다 한 바이트씩 보내도 타임아웃을 무기한 갱신하지 못한다.
통합 테스트는 연결 중단·분할 요청·조금씩 보내는 미완성 요청 중 종료를 확인한다.

## 상품 수정과 HTTP 캐시 경로

`demo_dispatch` → `dispatch_product` → `get_product`는
`cache_client_get` → `request_cache` → `http_request`로 미니 레디스에
`GET /keys/product%3A42`를 전송한다. 캐시가 있으면 원문 상품값으로 같은 JSON 응답을
재구성하고 `X-Cache:HIT`를 붙인다. 응답 껍질까지 저장해 1MiB 한도를 넘기지 않도록
**상품값 문자열만** 캐시에 넣는다.

부재·캐시 연결 실패·제한시간 초과는 `slow_source_get_product`로 간다.
기본 300ms 후 값을 받아 응답을 만들고 `cache_client_set`이 `PUT /keys/...`와
기본 TTL 60000ms를 HTTP로 전송한다. 캐시 저장 실패여도 원본 성공 응답은 반환한다.
`?cache=off`는 캐시 조회·저장을 모두 건너뛴다. 원본 조회 시도는 mutex 아래 집계한다.
원본 `fail` ID는 실패 재현용이며 503은 캐시하지 않는다. 첫 조회 MISS 후 재조회 HIT이면
`/demo/stats`의 원본 조회 횟수는 그대로다.

`PUT /products/42`의 `update_product` → `apply_update`는 검증 후
`slow_source_update_product`로 원본을 바꾸고 `cache_client_delete`로
`DELETE /keys/product%3A42`까지 완료한 뒤 `{"updated":true}`를 반환한다.
다음 GET은 MISS이고 새 원본값을 캐시에 채운다. 캐시 장애 시 상품 GET은 계속 원본으로
응답한다. 상품 PUT의 원본 변경 후 삭제 실패는 503 `CACHE_UNAVAILABLE`이며 원본 변경은
롤백하지 않는다. 실패를 성공으로 표시하지 않는 선택이다.

## 시간 복잡도와 한계

| 연산 | 평균 | 최악/비용 |
|---|---|---|
| set/get/delete/exists/expire/ttl | O(1), 문자열 처리 길이 별도 | 충돌 O(n) |
| 신규 set 확장 | 분할상환 O(1) | 재배치 O(capacity+n), 일시 정지 |
| prefix 무효화, size, stats | O(capacity+n) | 전체 store mutex 점유 |
| 주기적 정리 | 삭제 최대 200개 | 검사 자체는 전체 순회 O(capacity+n) |
| demo 원본 수정 목록 조회 | O(n) | 실제 DB 대신 학습용 연결 리스트 |

버킷 축소, 점진적 rehash, 최대 메모리 제한·LRU, 인증/TLS, keep-alive,
chunked, 영속성은 구현하지 않았다. 만료 정리는 삭제 수만 제한하므로 큰 테이블의 검사 지연은
남는다. JSON은 깊이 32와 최상위 필드 16개를 제한하고 초과 필드는 거부한다.
JSON 이스케이프 후 본문이 2MiB를 넘으면 값이 1MiB 이하라도 전송 한도가 우선한다.
캐시 저장에 실패하면 원본 응답을 제공한다. 같은 MISS 키 8개 동시 요청에서는 원본 8회가
관찰되었다. stampede 방지와 갱신·조회가 겹칠 때의 캐시 재충전 일관성은 선택 범위다.
이번 검증은 순차적 갱신·무효화 계약을 보장한다. 다른 팀원 구현의 테스트와 Linux 실행은
이 환경에서 수행하지 않았으므로 팀 통합 때 확인해야 한다.

## 영속성 비교 — 구현은 선택으로 남김

| 방식 | 기록·복구 | 장점 | 한계 |
|---|---|---|---|
| 스냅샷 | 살아 있는 전체 항목을 임시 파일로 쓰고 rename; 재시작 시 전체 로드 | 작고 빠른 복구 | 마지막 스냅샷 이후 변경 손실, 저장 시 O(n) 비용 |
| AOF | 성공한 set/delete/expire/prefix 삭제를 적용 순서대로 JSONL 추가; 시작 시 공개 store 함수로 순차 재적용 | fsync 주기에 따라 손실 구간 감소 | 로그 증가, 긴 복구, fsync 지연과 재작성 필요 |
| 둘 다 | 스냅샷 기준점 뒤의 로그만 재적용 | 복구 속도와 손실 구간 절충 | 원자적 기준점과 파일 교체가 복잡 |

원본에서 다시 만들 수 있는 이번 캐시는 영속성 없이도 동작한다. 데이터를 원본으로 쓴다면
AOF 매번 fsync는 손실을 줄이는 대신 쓰기 지연을 늘리고, 1초마다 fsync는 최근 약 1초 손실을
허용해 처리량을 높인다. 정확한 손실 허용 범위는 팀에서 결정해야 한다.
구현 시 단조 만료 시각을 실제 시각의 expiresAtWallMs로 바꿔 기록하고, 복구 시 이미 만료된
항목은 제외한다. 복구 중 재기록은 금지하고 잘린 마지막 줄은 제외·경고한다. 스냅샷·재작성은
임시 파일+rename을 사용하며 기록 순서와 쓰기 명령 적용 순서를 같은 동기화 경계로 보장한다.

## 실측과 새 내부 이름

같은 상품 반복 조회 평균은 OFF 305.477ms / ON 6.766ms, 적중률 98.33%,
원본은 60회에서 1회로 줄었다. 새 상품만 조회하면 OFF 305.255ms / ON 305.742ms로
적중이 없어 HTTP 캐시 오버헤드가 추가됐다. 60요청·동시성4·3회·원본300ms·TTL60000ms,
매번 빈 초기 상태다. 자세한 조건·p95·처리량은 benchmark.md에 있다.

공통 store.h와 필수 REST 이름/매개변수/반환 규칙은 변경하지 않았다.
내부 이름은 `hash_entry_t`, `hash_table_find_entry/put/remove_entry/visit`,
`find_live_entry`, `is_expired`, `server_context_t`, `route_t`, `store_result_t`,
`json_object_t`, `json_parse_object/quote`, `http_stream_t`, `http_client_request_t`,
`http_serve/request/read/write`, `handlers_dispatch`, `router_dispatch`, `cache_client_t`,
`cache_client_get/set/delete`, `slow_source_t`, `slow_source_get_product/update_product`다.
주요 파일 위치는 README에 적었다. 협약 자체는 수정하지 않았다.

핵심 설명에 참고할 파일은 `hash_table.c`, `store.c`, `handlers.c`, `sweeper.c`,
`cache_client.c`, `sample_api.c`다. 팀 검토에서 직접 답할 질문:

1. `product:42`가 충돌하고 rehash될 때에도 다른 키와 독립적으로 조회되는 이유와
   FNV-1a → 나머지 → 체인 비교 과정을 설명할 수 있는가?
2. 만료 판단·지연 삭제·주기적 정리를 같은 mutex 안에 두어야 하는 이유와,
   재저장한 값을 과거 만료 판정으로 삭제하지 않는 근거는 무엇인가?
3. 값 덮어쓰기의 할당 실패와 TTL 제거를 어떻게 처리하며, 상품 수정 후 HTTP 캐시 삭제를
   완료하기 전에 성공을 응답하면 어떤 일이 생기는가?

## 별도 코드 검토와 경계 회귀 결과

별도 에이전트가 협약과 현재 변경을 읽고 검토했으며, 다음 항목은 실패 재현 후 수정했다.

| 항목 | 재현 | 최종 검증 |
|---|---|---|
| URL 디코딩 할당 실패가 400으로 변환됨 | `test_http_allocation`의 500 기대 실패 | ENOMEM 구분 후 500 통과 |
| 최대 상품값이 캐시 JSON 포장 때문에 한도 초과 | 1MiB 값 두 번째 GET도 MISS | 상품 원문 저장 후 HIT, 원격 값 확인 |
| 본문을 조금씩 보내며 종료를 무기한 지연 | SIGTERM 후 5초 wait 실패 | 절대 제한시간 적용 후 통합 검사 통과 |
| 정밀 소수 TTL이 double에서 정수로 반올림 | `1.00000000000000000001`이 200 | 숫자 원문 자릿수 검증 후 400 |
| JSON 본문의 원시 NUL을 헤더 오류로 분류 | INVALID_JSON 대신 INVALID_BODY | 헤더와 본문 구분 후 INVALID_JSON |

C 테스트는 store 1,621개 검사와 할당 실패·JSON·sweeper·HTTP OOM의 5개 실행 파일이다.
외부 API는 7개 테스트(내부에 다수의 경계·동시 요청 assertion), 데모는 캐시 전체 생명주기
통합 테스트다. ASan·UBSan의 C 테스트와 두 서버 실제 HTTP 테스트 모두 실행했다.
macOS LeakSanitizer 미지원은 숨기지 않고 README에 기록했다.
