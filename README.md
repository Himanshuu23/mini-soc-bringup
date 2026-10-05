picosoc-bringup

A small RV32I SoC built around PicoRV32, with a UART, a timer and a DMA engine, plus bare-metal C firmware and a Verilator testbench. Boots firmware in simulation, takes interrupts, copies memory with DMA, and reports pass/fail through a magic exit register. Python is used to poke registers over the simulated UART.

What It Does

Builds firmware with a bare-metal toolchain, loads it into the simulated RAM, runs it on the real CPU RTL and reports each test over UART.

    fw/*.c, start.S
         |  riscv gcc -march=rv32i
         v
    firmware .bin  -->  sim/tb.cpp loads it into RAM
                              |
                              v
                        Verilator model of soc_top
                              |
              uart_tx --> decoded bit by bit --> terminal
              sim_exit register --> process exit code

Architecture

    +-----------+  mem_valid/mem_ready   +-----------------------------+
    | PicoRV32  |<---------------------->|  address decoder (soc_top)  |
    | ENABLE_IRQ|                        +--+-------+-------+-------+--+
    +-----^-----+                           |       |       |       |
          | irq[1:0]                        |       |       |       |
          |                              +--v--+ +--v--+ +--v--+ +--v--+
          |                              | RAM | |UART | |TIMER| | DMA |
          |                              |64 KB| |     | |     | |     |
          |                              +--^--+ +-----+ +--+--+ +-+-+-+
          |                                 |               |      | |
          |                          arbiter (CPU / DMA)    |      | |
          |                                 +---------------+------+ |
          +----------- timer irq (bit 0), dma irq (bit 1) -----------+

The DMA is a second bus master on the RAM. A small arbiter in soc_top alternates between the CPU and the DMA when both want the RAM, so neither starves.

Memory Map

| Range | Device |
|---|---|
| 0x0000_0000 - 0x0000_FFFF | RAM, 64 KB |
| 0x1000_0000 | UART |
| 0x1000_1000 | TIMER |
| 0x1000_2000 | DMA |
| 0x1000_3000 | SYS |

Firmware splits the 64 KB in the linker script: code, rodata and the .data load image in the low 32 KB, .data, .bss and the stack in the high 32 KB.

Boot Flow

    reset, PC = 0x0000_0000
      -> vectors: "j reset_handler" at 0x00, irq_entry at 0x10
      -> reset_handler (start.S)
           sp = _stack_top
           zero .bss
           copy .data from its load address in the low half
           call main
      -> main returns a status
      -> start.S writes it to the SYS exit register
      -> testbench stops and returns that status as the process exit code

PicoRV32 comes out of reset with every interrupt masked, so firmware unmasks the ones it wants with the maskirq instruction.

Interrupt Handling

    peripheral raises its level irq (timer match, dma done)
      -> PicoRV32 finishes the current instruction, saves the return PC in q0 and the pending mask in q1
      -> jumps to 0x10 (irq_entry)
      -> irq_entry saves x1..x31 and the return PC to irq_regs
      -> a0 = pending mask, calls irq_handler(pending) in C
      -> timer_isr / dma_isr read their status, write 1 to clear the flag, bump a counter
      -> irq_entry restores registers, retirq jumps back to q0

Each peripheral irq is a level signal that stays high until the status flag is cleared, so the handler must clear it before returning. Timer is irq bit 0, DMA is irq bit 1.

Waveform: Timer Interrupt (Pending but Masked)

Captured with make sim TEST=timer_irq TRACE=1 and viewed in GTKWave.

![Timer IRQ overview](assets/timer_irq_waveform.png)

![Timer IRQ zoomed in](assets/timer_irq_waveform2.png)

timer_irq goes high and irq becomes 00000001 (bit 0 is the timer). The CPU
does not jump to irq_entry yet, because the firmware has not unmasked the
timer interrupt, so it stays pending. Meanwhile mem_addr shows the CPU
looping in main and reading a timer register (0x1000100C) while it polls the
status. The test fw/tests/timer_irq.c checks this on purpose ("masked cpu irq
stays pending"). Only after irq_enable(IRQ_TIMER) does the CPU take it.

DMA

A word-copy engine from RAM to RAM. Software writes SRC, DST, LEN (in words), sets CTRL.start and either polls STATUS.done or waits for the interrupt. Internally it is a three-state machine (idle, read, write); one word costs four cycles when the RAM is free. Measured in this repo's test: 64 words in about 397 cycles while the CPU is also fetching code from the same RAM.

Building

    make all-tests

Needs verilator, a RISC-V bare-metal gcc, make and python3. No cmake, no pip packages.

Install on Ubuntu 24.04:

    sudo apt install verilator gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf make python3 g++

The Makefile uses riscv32-unknown-elf-gcc if it is on PATH and otherwise falls back to riscv64-unknown-elf-gcc. Both are invoked with -march=rv32i -mabi=ilp32, and the Ubuntu riscv64 package ships an rv32i multilib. Override with make CROSS=my-prefix-.

On Arch Linux the compiler is named riscv64-elf-gcc, so run: make CROSS=riscv64-elf- all-tests

Usage

    make fw                     build all firmware images into build/fw
    make sim TEST=dma_memcpy    run one test
    make sim TEST=timer_irq TRACE=1
                                same, and write build/timer_irq.vcd
    make all-tests              run everything and print a summary
    make regtool-test           python register tool self-test
    make clean

Testbench flags (build/sim/soc_sim):

| Flag | Meaning |
|---|---|
| --trace | write a VCD |
| --vcd FILE | VCD path, default trace.vcd |
| --trace-cycles N | stop dumping after N cycles, default 50000 (a full VCD is large) |
| --timeout N | give up after N cycles and return 124, default 5000000 |
| --listen PORT | serve the UART on a TCP socket instead of stdout |

The testbench exits with the firmware's exit code, 99 if the CPU traps, 124 on timeout.

Tests

| Test | What it checks |
|---|---|
| uart_hello | TX path, STATUS bits, BAUD register |
| timer_irq | count, compare, W1C status, masked irq stays pending, exactly N interrupts |
| dma_memcpy | polled copy, copy with done interrupt, zero length, writes ignored while busy, guard words, CPU progress during the copy |
| boot_check | .data initialised, .bss zeroed, .rodata, stack placement |

The testbench fills RAM with 0xdeadbeef before loading the image, so a missing .bss clear shows up as garbage instead of passing by luck.

Register Tool

tools/regtool.py talks to a monitor firmware (fw/apps/monitor.c) running on the simulated SoC. The testbench exposes the SoC UART on a TCP port and the tool speaks a line protocol over it.

| Command | Reply |
|---|---|
| R addr | 8 hex digits |
| W addr value | OK |
| P | OK |
| Q | BYE, ends the simulation |

Addresses and values are hex, word aligned. Bad input gets ERR.

    $ python3 tools/regtool.py "R 10000008" "W 10001004 abcd" "R 10001004"
    R 0x10000008 = 0x00000010
    W 0x10001004 <- 0x0000abcd
    R 0x10001004 = 0x0000abcd

It starts the simulator itself. Use --port N to attach to one you started with --listen N, --shell for an interactive prompt, --selftest for the check that runs in make all-tests (RAM, timer, and a DMA transfer set up entirely from Python).

From Python:

    from regtool import RegBridge
    bridge = RegBridge()
    bridge.write_reg(0x10001004, 1000)
    print(hex(bridge.read_reg(0x10001004)))
    bridge.close()

Problems Hit During Bring-up

- Every interrupt was handled twice. PicoRV32 latches pending irqs by default (LATCHED_IRQ), but the peripherals hold their irq high until software clears them, so the latch was set again while the handler was running and the CPU re-entered it right after retirq. Fix: LATCHED_IRQ = 0xfffffffc, so bits 0 and 1 follow the line.
- The last UART character was lost. The firmware wrote the exit code while the final byte was still shifting out. sys_exit now waits for tx_busy to clear.
- boot_check failed on a correct .bss. It was scanning all of .bss, which includes the test framework's own variables. It now checks the test's own arrays.
- The DMA timing test measured UART printing, not the DMA. 64 words take a few hundred cycles, one printed line takes thousands. Timing is now captured with no printing in the window.

Limitations

- Irq bit 1 is also PicoRV32's own ebreak/illegal-instruction interrupt, and the DMA owns that bit here. With IRQ_DMA masked, an illegal instruction traps and the testbench exits with code 99 (checked). With it unmasked, the event arrives as irq 1, dma_isr sees DONE clear and ignores it, and execution carries on (also checked). Firmware therefore keeps IRQ_DMA masked outside DMA code.
- UART RX has a single byte buffer and no FIFO. The testbench leaves two character times between injected bytes.
- The DMA only reaches RAM. Addresses outside 64 KB wrap onto it, there is no error flag.
- No RISC-V compliance suite and no formal checks. Coverage is the four firmware tests and the regtool self-test.

Layout

    rtl/soc_top.v, uart.v, timer.v, dma.v, ram.v
    rtl/third_party/picorv32.v   vendored, ISC license
    fw/start.S, link.ld          reset, irq entry, linker script
    fw/uart.c timer.c dma.c      drivers on volatile register structs
    fw/irq.c                     C interrupt dispatcher
    fw/tests/                    the four tests
    fw/apps/monitor.c            command monitor for regtool
    sim/tb.cpp                   Verilator testbench
    sim/run_tests.sh             summary table
    tools/regtool.py
