# fw/test/Makefile — host unit tests for the pure (HAL-free) layers:
# regulation compute (app_reg_calc) + Modbus core (app_modbus_core).
# Runs on the host toolchain. Usage: make -C fw/test test
CC     ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Wundef -Wshadow -O0 -g
INC    := -I../include
BIN_REG := /tmp/gds_test_app_reg_calc
BIN_MB  := /tmp/gds_test_app_modbus_core
BIN_TCP  := /tmp/gds_test_app_modbus_tcp_frame
BIN_WELD := /tmp/gds_test_app_weld_fsm
BIN_SR  := /tmp/gds_test_app_seek_reset_fsm
BIN_POT := /tmp/gds_test_i2c_pot

test: $(BIN_REG) $(BIN_MB) $(BIN_TCP) $(BIN_WELD) $(BIN_SR) $(BIN_POT)
	$(BIN_REG)
	$(BIN_MB)
	$(BIN_TCP)
	$(BIN_WELD)
	$(BIN_SR)
	$(BIN_POT)

$(BIN_REG): test_app_reg_calc.c ../src/app_reg_calc.c ../include/app_reg_calc.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_reg_calc.c ../src/app_reg_calc.c

$(BIN_MB): test_app_modbus_core.c ../src/app_modbus_core.c ../include/app_modbus_core.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_modbus_core.c ../src/app_modbus_core.c

$(BIN_TCP): test_app_modbus_tcp_frame.c ../src/app_modbus_tcp_frame.c ../src/app_modbus_core.c ../include/app_modbus_tcp_frame.h ../include/app_modbus_core.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_modbus_tcp_frame.c ../src/app_modbus_tcp_frame.c ../src/app_modbus_core.c

$(BIN_WELD): test_app_weld_fsm.c ../src/app_weld_fsm.c ../include/app_weld_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_weld_fsm.c ../src/app_weld_fsm.c

$(BIN_SR): test_app_seek_reset_fsm.c ../src/app_seek_reset_fsm.c ../include/app_seek_reset_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_seek_reset_fsm.c ../src/app_seek_reset_fsm.c

# i2c_pot: pure conversion is a HAL-free static inline in i2c_pot.h — test
# includes the header only (no .c to compile). i2c_pot_set_dac (HAL) is HW-gated.
$(BIN_POT): test_i2c_pot.c ../include/i2c_pot.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_i2c_pot.c

clean:
	rm -f $(BIN_REG) $(BIN_MB) $(BIN_TCP) $(BIN_WELD) $(BIN_SR) $(BIN_POT)

.PHONY: test clean
