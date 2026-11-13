CC      = cc
EXTRA_CFLAGS =
CFLAGS  = -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L \
          -Isrc/store -Isrc/server -Isrc/demo -Isrc/client $(EXTRA_CFLAGS)
LDFLAGS = -lpthread
PYTHON  = python3

BUILD_DIR = build

HEADERS     = $(wildcard src/*/*.h)
STORE_SRCS  = src/store/store.c src/store/hash_table.c src/store/clock.c
# store를 모르는 공용 모듈 : HTTP 읽기·쓰기, JSON, 연결 받기와 워커 풀
HTTP_SRCS   = src/server/http.c src/server/json.c src/server/tcp_server.c
SERVER_SRCS = src/server/main.c src/server/router.c src/server/handlers.c \
              src/server/api_errors.c src/server/sweeper.c $(HTTP_SRCS)
SERVER_LIB_SRCS = $(filter-out src/server/main.c,$(SERVER_SRCS))
CLIENT_SRCS = src/client/mini_redis_client.c
DEMO_SRCS   = src/demo/sample_api.c src/demo/slow_source.c $(CLIENT_SRCS) $(HTTP_SRCS)

STORE_TEST_SRCS  = $(wildcard tests/store/test_*.c)
SERVER_TEST_SRCS = $(wildcard tests/server/test_*.c)
STORE_TEST_BINS  = $(patsubst tests/store/%.c,$(BUILD_DIR)/%,$(STORE_TEST_SRCS))
SERVER_TEST_BINS = $(patsubst tests/server/%.c,$(BUILD_DIR)/%,$(SERVER_TEST_SRCS))
TEST_BINS        = $(STORE_TEST_BINS) $(SERVER_TEST_BINS)

SERVER_BIN      = $(BUILD_DIR)/mini_redis
DEMO_BIN        = $(BUILD_DIR)/sample_api
BENCH_STORE_BIN = $(BUILD_DIR)/bench_store
API_TEST_PORT   = 16380

.PHONY: all test test-asan test-api test-demo test-all bench bench-store run run-demo clean

all: $(SERVER_BIN) $(DEMO_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(SERVER_BIN): $(STORE_SRCS) $(SERVER_SRCS) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDFLAGS)

# demo는 store를 직접 링크하지 않는다 (협약 6장). 미니 레디스에는 src/client로 HTTP 요청만 보낸다
$(DEMO_BIN): $(DEMO_SRCS) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDFLAGS)

$(STORE_TEST_BINS): $(BUILD_DIR)/%: tests/store/%.c $(STORE_SRCS) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDFLAGS)

$(SERVER_TEST_BINS): $(BUILD_DIR)/%: tests/server/%.c $(SERVER_LIB_SRCS) $(STORE_SRCS) \
                     $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDFLAGS)

# 해시 테이블 자체 성능 측정 (협약 15.4 권장). 6장의 "bench는 store를 링크하지 않는다"의 예외다
$(BENCH_STORE_BIN): bench/bench_store.c $(STORE_SRCS) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -O2 -o $@ $(filter %.c,$^) $(LDFLAGS)

# 단위 테스트 (store, hash_table, json, http)
test: $(TEST_BINS)
	@for test_bin in $(TEST_BINS); do echo "== $$test_bin"; ./$$test_bin || exit 1; done

# 메모리 오류·누수 검사 (협약 4.5). 결과물은 build/asan에 따로 둔다
test-asan:
	$(MAKE) test BUILD_DIR=$(BUILD_DIR)/asan EXTRA_CFLAGS="-g -fsanitize=address,undefined"

# 외부 기능 테스트 (협약 15.2, 15.3). 서버를 띄워 테스트한 뒤 SIGTERM으로 정상 종료하는지도 본다
test-api: $(SERVER_BIN)
	@./$(SERVER_BIN) $(API_TEST_PORT) > $(BUILD_DIR)/test_api_server.log 2>&1 & \
	server_pid=$$!; \
	$(PYTHON) tests/api/test_api.py http://127.0.0.1:$(API_TEST_PORT); test_status=$$?; \
	kill -TERM $$server_pid; wait $$server_pid; server_status=$$?; \
	echo "서버 종료 코드 : $$server_status"; \
	[ $$test_status -eq 0 ] && [ $$server_status -eq 0 ]

# 캐시 샘플 API 테스트 (협약 15.4). 미니 레디스를 끄는 경우도 있어 테스트가 서버를 직접 띄운다
test-demo: $(SERVER_BIN) $(DEMO_BIN)
	$(PYTHON) tests/demo/test_demo.py $(SERVER_BIN) $(DEMO_BIN)

test-all: test test-asan test-api test-demo

# 캐시 ON/OFF 성능 비교 (협약 15.4). 결과는 bench/results/에 남는다
bench: $(SERVER_BIN) $(DEMO_BIN)
	$(PYTHON) bench/run_bench.py $(SERVER_BIN) $(DEMO_BIN)

bench-store: $(BENCH_STORE_BIN)
	./$(BENCH_STORE_BIN)

run: $(SERVER_BIN)
	./$(SERVER_BIN)

run-demo: $(DEMO_BIN)
	./$(DEMO_BIN)

clean:
	rm -rf $(BUILD_DIR)
