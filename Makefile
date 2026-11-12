CC      = cc
.DEFAULT_GOAL := all
CFLAGS  = -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -Isrc/store -Isrc/server -Isrc/demo -Isrc/client
LDFLAGS = -lpthread -lm

BUILD_DIR = build

STORE_SRCS  = src/store/store.c src/store/hash_table.c src/store/clock.c
SERVER_SRCS = src/server/main.c src/server/http.c src/server/router.c \
              src/server/handlers.c src/server/api_errors.c src/server/sweeper.c src/server/json.c
DEMO_SRCS   = src/demo/sample_api.c src/demo/slow_source.c src/client/cache_client.c \
              src/server/http.c src/server/json.c
TEST_SRCS   = $(filter-out tests/store/test_allocation.c,$(wildcard tests/store/test_*.c))

SERVER_BIN = $(BUILD_DIR)/mini_redis
DEMO_BIN   = $(BUILD_DIR)/sample_api
TEST_BINS = $(patsubst tests/store/%.c,$(BUILD_DIR)/%,$(TEST_SRCS)) \
            $(BUILD_DIR)/test_allocation $(BUILD_DIR)/test_json \
            $(BUILD_DIR)/test_sweeper $(BUILD_DIR)/test_http_allocation

.PHONY: all test run run-demo clean

all: $(SERVER_BIN) $(DEMO_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

HEADERS = $(wildcard src/store/*.h src/server/*.h src/demo/*.h src/client/*.h)

$(SERVER_BIN): $(STORE_SRCS) $(SERVER_SRCS) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDFLAGS)

# demo는 store를 직접 링크하지 않는다 (협약 6장)
$(DEMO_BIN): $(DEMO_SRCS) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDFLAGS)

$(BUILD_DIR)/test_%: tests/store/test_%.c $(STORE_SRCS) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDFLAGS)

test: $(TEST_BINS)
	@for test_bin in $(TEST_BINS); do echo "== $$test_bin"; ./$$test_bin || exit 1; done

run: $(SERVER_BIN)
	./$(SERVER_BIN)

run-demo: $(DEMO_BIN)
	./$(DEMO_BIN)

clean:
	rm -rf $(BUILD_DIR)

FAULT_OBJS = $(patsubst src/store/%.c,$(BUILD_DIR)/fault_%.o,$(STORE_SRCS))

$(BUILD_DIR)/fault_%.o: src/store/%.c tests/store/fail_alloc.h $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -include tests/store/fail_alloc.h -c $< -o $@

$(BUILD_DIR)/test_allocation: tests/store/test_allocation.c $(FAULT_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_json: tests/server/test_json.c src/server/json.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDFLAGS)

$(BUILD_DIR)/test_sweeper: tests/server/test_sweeper.c src/server/sweeper.c $(STORE_SRCS) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDFLAGS)

$(BUILD_DIR)/fault_http.o: src/server/http.c tests/store/fail_alloc.h $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -include tests/store/fail_alloc.h -c $< -o $@

$(BUILD_DIR)/test_http_allocation: tests/server/test_http_allocation.c $(BUILD_DIR)/fault_http.o \
    src/server/router.c src/server/handlers.c src/server/api_errors.c src/server/json.c \
    $(STORE_SRCS) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.c %.o,$^) $(LDFLAGS)

.PHONY: test-http test-asan bench

test-http: all
	python3 tests/api/run_local.py

test-asan:
	ASAN_OPTIONS=detect_leaks=0 $(MAKE) BUILD_DIR=build/asan \
	  CFLAGS='$(CFLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
	  LDFLAGS='$(LDFLAGS) -fsanitize=address,undefined' all test

bench: all
	python3 bench/compare.py
