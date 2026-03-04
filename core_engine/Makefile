# ArborFlow — core_engine Makefile
# Member 2: Gatekeeper Module
# -----------------------------------------------
# Usage:
#   make          — build the test binary
#   make test     — build and run the test suite
#   make clean    — remove build artifacts

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11 -Iinclude
SRCDIR  = src
SRCS    = $(SRCDIR)/bit_vector.c \
          $(SRCDIR)/veb_tree.c   \
          $(SRCDIR)/gatekeeper.c
OBJS    = $(SRCS:.c=.o)

# Test binary
TEST_SRC  = tests/test_gatekeeper.c
TEST_BIN  = gatekeeper_test

all: $(TEST_BIN)

$(TEST_BIN): $(OBJS) $(TEST_SRC)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(TEST_SRC)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(OBJS) $(TEST_BIN)

.PHONY: all test clean
