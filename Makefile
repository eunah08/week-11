CC      = cc
CFLAGS  = -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -Isrc/store -Isrc/server -Isrc/demo
LDFLAGS = -lpthread

BUILD_DIR = build

STORE_SRCS  = src/store/store.c src/store/hash_table.c src/store/clock.c
SERVER_SRCS = src/server/main.c src/server/http.c src/server/router.c \
              src/server/handlers.c src/server/api_errors.c src/server/sweeper.c
DEMO_SRCS   = src/demo/sample_api.c src/demo/slow_source.c
TEST_SRCS   = $(wildcard tests/store/test_*.c)

SERVER_BIN = $(BUILD_DIR)/mini_redis
DEMO_BIN   = $(BUILD_DIR)/sample_api
TEST_BINS  = $(patsubst tests/store/%.c,$(BUILD_DIR)/%,$(TEST_SRCS))

.PHONY: all test run run-demo clean

all: $(SERVER_BIN) $(DEMO_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(SERVER_BIN): $(STORE_SRCS) $(SERVER_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# demo는 store를 직접 링크하지 않는다 (협약 6장)
$(DEMO_BIN): $(DEMO_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_%: tests/store/test_%.c $(STORE_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: $(TEST_BINS)
	@for test_bin in $(TEST_BINS); do echo "== $$test_bin"; ./$$test_bin || exit 1; done

run: $(SERVER_BIN)
	./$(SERVER_BIN)

run-demo: $(DEMO_BIN)
	./$(DEMO_BIN)

clean:
	rm -rf $(BUILD_DIR)
