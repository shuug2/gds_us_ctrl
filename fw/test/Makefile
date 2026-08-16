# fw/test/Makefile — host unit tests for the HAL-free layers.
# Suites: reg_calc / modbus_core / tcp_frame / weld_fsm / seek_reset_fsm /
#         i2c_pot / app_config(mock-fram) / freq_fsm / buzzer_fsm / overload_fsm /
#         input_fsm / osc_init_fsm / weld_trigger_fsm / horn_fsm / remote_en_fsm.
# Usage: make -C fw/test test
CC     ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Wundef -Wshadow -O0 -g
INC    := -I../include
BIN_REG := /tmp/gds_test_app_reg_calc
BIN_MB  := /tmp/gds_test_app_modbus_core
BIN_TCP  := /tmp/gds_test_app_modbus_tcp_frame
BIN_WELD := /tmp/gds_test_app_weld_fsm
BIN_SR  := /tmp/gds_test_app_seek_reset_fsm
BIN_POT := /tmp/gds_test_i2c_pot
BIN_CFG := /tmp/gds_test_app_config
BIN_FQ  := /tmp/gds_test_app_freq_fsm
BIN_BZ  := /tmp/gds_test_app_buzzer_fsm
BIN_OL  := /tmp/gds_test_app_overload_fsm
BIN_IN  := /tmp/gds_test_app_input_fsm
BIN_OSC := /tmp/gds_test_app_osc_init_fsm
BIN_WTR := /tmp/gds_test_app_weld_trigger_fsm
BIN_HORN := /tmp/gds_test_app_horn_fsm
BIN_REN := /tmp/gds_test_app_remote_en_fsm

test: $(BIN_REG) $(BIN_MB) $(BIN_TCP) $(BIN_WELD) $(BIN_SR) $(BIN_POT) $(BIN_CFG) $(BIN_FQ) $(BIN_BZ) $(BIN_OL) $(BIN_IN) $(BIN_OSC) $(BIN_WTR) $(BIN_HORN) $(BIN_REN)
	$(BIN_REG)
	$(BIN_MB)
	$(BIN_TCP)
	$(BIN_WELD)
	$(BIN_SR)
	$(BIN_POT)
	$(BIN_CFG)
	$(BIN_FQ)
	$(BIN_BZ)
	$(BIN_OL)
	$(BIN_IN)
	$(BIN_OSC)
	$(BIN_WTR)
	$(BIN_HORN)
	$(BIN_REN)

$(BIN_HORN): test_app_horn_fsm.c ../src/app_horn_fsm.c ../include/app_horn_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_horn_fsm.c ../src/app_horn_fsm.c

$(BIN_REN): test_app_remote_en_fsm.c ../src/app_remote_en_fsm.c ../include/app_remote_en_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_remote_en_fsm.c ../src/app_remote_en_fsm.c

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

# app_config: fram is mock_fram.c (fw/drivers/fram.c needs HAL — do NOT link it here)
$(BIN_CFG): test_app_config.c mock_fram.c mock_fram.h ../src/app_config.c ../include/app_config.h ../include/fram.h
	$(CC) $(CFLAGS) $(INC) -I. -o $@ test_app_config.c mock_fram.c ../src/app_config.c

$(BIN_FQ): test_app_freq_fsm.c ../src/app_freq_fsm.c ../include/app_freq_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_freq_fsm.c ../src/app_freq_fsm.c

$(BIN_BZ): test_app_buzzer_fsm.c ../src/app_buzzer_fsm.c ../include/app_buzzer_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_buzzer_fsm.c ../src/app_buzzer_fsm.c

$(BIN_OL): test_app_overload_fsm.c ../src/app_overload_fsm.c ../include/app_overload_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_overload_fsm.c ../src/app_overload_fsm.c

$(BIN_IN): test_app_input_fsm.c ../src/app_input_fsm.c ../include/app_input_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_input_fsm.c ../src/app_input_fsm.c

$(BIN_OSC): test_app_osc_init_fsm.c ../src/app_osc_init_fsm.c ../include/app_osc_init_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_osc_init_fsm.c ../src/app_osc_init_fsm.c

$(BIN_OL): test_app_overload_fsm.c ../src/app_overload_fsm.c ../include/app_overload_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_overload_fsm.c ../src/app_overload_fsm.c

$(BIN_IN): test_app_input_fsm.c ../src/app_input_fsm.c ../include/app_input_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_input_fsm.c ../src/app_input_fsm.c

$(BIN_OSC): test_app_osc_init_fsm.c ../src/app_osc_init_fsm.c ../include/app_osc_init_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_osc_init_fsm.c ../src/app_osc_init_fsm.c

$(BIN_WTR): test_app_weld_trigger_fsm.c ../src/app_weld_trigger_fsm.c ../include/app_weld_trigger_fsm.h ../include/app_weld_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_weld_trigger_fsm.c ../src/app_weld_trigger_fsm.c

clean:
	rm -f $(BIN_REG) $(BIN_MB) $(BIN_TCP) $(BIN_WELD) $(BIN_SR) $(BIN_POT) $(BIN_CFG) $(BIN_FQ) $(BIN_BZ) $(BIN_OL) $(BIN_IN) $(BIN_OSC) $(BIN_WTR) $(BIN_HORN) $(BIN_REN)

.PHONY: test clean
