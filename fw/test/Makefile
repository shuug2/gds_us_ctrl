# fw/test/Makefile — host unit tests for the pure regulation compute layer.
# Runs on the host toolchain (no ARM, no HAL). Usage: make -C fw/test test
CC     ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Wundef -Wshadow -O0 -g
INC    := -I../include
SRC    := ../src/app_reg_calc.c
BIN    := /tmp/gds_test_app_reg_calc

test: $(BIN)
	$(BIN)

$(BIN): test_app_reg_calc.c $(SRC) ../include/app_reg_calc.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_reg_calc.c $(SRC)

clean:
	rm -f $(BIN)

.PHONY: test clean
