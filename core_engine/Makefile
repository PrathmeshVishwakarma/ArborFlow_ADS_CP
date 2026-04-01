# ArborFlow — core_engine Makefile
# Member 2: Gatekeeper Module
# -----------------------------------------------
# Usage:
#   make          — build the test binary
#   make test     — build and run the test suite
#   make clean    — remove build artifacts

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11 -Iinclude -I/opt/homebrew/opt/libpcap/include
SRCDIR  = src
SRCS    = $(SRCDIR)/bit_vector.c \
          $(SRCDIR)/veb_tree.c   \
          $(SRCDIR)/gatekeeper.c \
          $(SRCDIR)/concurrent_q.c \
          $(SRCDIR)/capture.c
		  $(SRCDIR)/gatekeeper.c \
		  $(SRCDIR)/splay_tree.c
OBJS    = $(SRCS:.c=.o)

# Test binary
TEST_SRC  = tests/test_gatekeeper.c
TEST_BIN  = gatekeeper_test

# IPC Engine binary
IPC_SRC   = $(SRCDIR)/ipc_engine.c
IPC_BIN   = ipc_engine

# Capture Demo binary
CAPTURE_SRC = tests/capture_demo.c
CAPTURE_BIN = capture_test

# Automatically add -lws2_32 on Windows for network functions
ifeq ($(OS),Windows_NT)
    LDFLAGS += -lws2_32
endif

# Add libpcap
LDFLAGS += -L/opt/homebrew/opt/libpcap/lib -lpcap -pthread

all: $(TEST_BIN) $(IPC_BIN) $(CAPTURE_BIN)

$(TEST_BIN): $(OBJS) $(TEST_SRC)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(TEST_SRC)

$(IPC_BIN): $(OBJS) $(IPC_SRC)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(IPC_SRC) $(LDFLAGS)

$(CAPTURE_BIN): $(OBJS) $(CAPTURE_SRC)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(CAPTURE_SRC) $(LDFLAGS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

capture_demo: $(CAPTURE_BIN)
	./$(CAPTURE_BIN)

clean:
	rm -f $(OBJS) $(TEST_BIN) $(IPC_BIN) $(CAPTURE_BIN) $(IPC_BIN).exe

.PHONY: all test clean
