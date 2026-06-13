# fw/test/Makefile — host unit tests for the pure (HAL-free) layers:
# regulation compute (app_reg_calc) + Modbus core (app_modbus_core).
# Runs on the host toolchain. Usage: make -C fw/test test
CC     ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Wundef -Wshadow -O0 -g
INC    := -I../include
BIN_REG := /tmp/gds_test_app_reg_calc
BIN_MB  := /tmp/gds_test_app_modbus_core
BIN_TCP := /tmp/gds_test_app_modbus_tcp_frame

test: $(BIN_REG) $(BIN_MB) $(BIN_TCP)
	$(BIN_REG)
	$(BIN_MB)
	$(BIN_TCP)

$(BIN_REG): test_app_reg_calc.c ../src/app_reg_calc.c ../include/app_reg_calc.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_reg_calc.c ../src/app_reg_calc.c

$(BIN_MB): test_app_modbus_core.c ../src/app_modbus_core.c ../include/app_modbus_core.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_modbus_core.c ../src/app_modbus_core.c

$(BIN_TCP): test_app_modbus_tcp_frame.c ../src/app_modbus_tcp_frame.c ../src/app_modbus_core.c ../include/app_modbus_tcp_frame.h ../include/app_modbus_core.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_modbus_tcp_frame.c ../src/app_modbus_tcp_frame.c ../src/app_modbus_core.c

clean:
	rm -f $(BIN_REG) $(BIN_MB) $(BIN_TCP)

.PHONY: test clean
