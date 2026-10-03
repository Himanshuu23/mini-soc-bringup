CROSS ?= $(shell command -v riscv32-unknown-elf-gcc >/dev/null 2>&1 && echo riscv32-unknown-elf- || echo riscv64-unknown-elf-)
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump

TESTS ?= uart_hello boot_check
TEST  ?= uart_hello
TRACE ?= 0

BUILD  := build
FW_OUT := $(BUILD)/fw
SIM_OUT := $(BUILD)/sim
SIM    := $(SIM_OUT)/soc_sim

ARCH    := -march=rv32i -mabi=ilp32
CFLAGS  := $(ARCH) -Os -g -Wall -Wextra -ffreestanding -fno-tree-loop-distribute-patterns -mno-relax -Ifw
LDFLAGS := $(ARCH) -nostdlib -nostartfiles -Wl,--gc-sections -Wl,--no-relax -T fw/link.ld

FW_COMMON := fw/start.S $(wildcard fw/*.c)
FW_DEPS   := $(FW_COMMON) $(wildcard fw/*.h) fw/link.ld
RTL       := $(wildcard rtl/*.v) rtl/third_party/picorv32.v

SIM_ARGS := --timeout 5000000
ifeq ($(TRACE),1)
SIM_ARGS += --trace --vcd $(BUILD)/$(TEST).vcd
endif

.PHONY: all fw sim all-tests clean check-tools
.SECONDARY:

all: all-tests

fw: $(addprefix $(FW_OUT)/,$(addsuffix .bin,$(TESTS)))

$(FW_OUT)/%.elf: fw/tests/%.c $(FW_DEPS)
	@mkdir -p $(FW_OUT)
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=$(FW_OUT)/$*.map -o $@ $(FW_COMMON) $< -lgcc

$(FW_OUT)/%.bin: $(FW_OUT)/%.elf
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -d -M no-aliases $< > $(FW_OUT)/$*.dis

$(SIM): $(RTL) sim/tb.cpp
	@mkdir -p $(SIM_OUT)
	verilator --cc --exe --build -j 0 --trace --public-flat-rw -Wno-lint -Wno-style -Wno-TIMESCALEMOD \
		--top-module soc_top --Mdir $(SIM_OUT) -o soc_sim \
		-CFLAGS "-O2" $(RTL) $(abspath sim/tb.cpp)

sim: $(SIM) $(FW_OUT)/$(TEST).bin
	$(SIM) $(SIM_ARGS) $(FW_OUT)/$(TEST).bin

all-tests: $(SIM) fw
	@sim/run_tests.sh $(SIM) $(FW_OUT) $(TESTS)

check-tools:
	@command -v verilator >/dev/null || echo "missing: verilator"
	@command -v $(CC) >/dev/null || echo "missing: $(CC)"
	@command -v python3 >/dev/null || echo "missing: python3"
	@echo "tools ok"

clean:
	rm -rf $(BUILD)
