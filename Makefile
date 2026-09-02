RISCV_TOOLCHAIN ?= /opt/tinyQV

CC = $(RISCV_TOOLCHAIN)/bin/riscv32-unknown-elf-gcc
AS = $(RISCV_TOOLCHAIN)/bin/riscv32-unknown-elf-as
AR = $(RISCV_TOOLCHAIN)/bin/riscv32-unknown-elf-ar

all: tinyQV.a tinyQV-sim.a start.o

clean:
	@rm -f *.o *.a fatfs/*.o sdcard/*.o

%.o: %.c 
	@echo "Compiling $(notdir $<)..."
	@$(CC) -O2 -march=rv32ec_zicsr_zcb_zicond_zilsd -mabi=ilp32e -nostdlib -nostartfiles -ffreestanding -ffunction-sections -fdata-sections -Wall -Werror -lc -I$(PWD) -c $< -o $@

%.o: %.s
	@echo "Assembling $(notdir $<)..."
	@$(AS) -march=rv32ec_zicsr_zcb_zicond -mabi=ilp32e $< -o $@

uart_sim.o: uart.c
	@echo "Compiling uart.c (sim)..."
	@$(CC) -DTINYQV_SIM -O2 -march=rv32ec_zcb_zicond_zilsd -mabi=ilp32e -nostdlib -nostartfiles -ffreestanding -ffunction-sections -fdata-sections -Wall -Werror -lc -I$(PWD) -c $< -o $@

uart_buf_sim.o: uart_buf.s
	@echo "Assembling uart_buf.s (sim)..."
	@$(AS) --defsym TINYQV_SIM=1 -march=rv32ec_zicsr_zcb_zicond -mabi=ilp32e $< -o $@

tinyQV.a: uart.o uart_buf.o mul.o isqrt.o peripheral.o runtime.o spi.o timer.o prism.o
	@echo "Archiving $@..."
	@$(AR) rcs $@ $^

tinyQV-sim.a: uart_sim.o uart_buf_sim.o mul.o isqrt.o peripheral.o runtime.o spi.o timer.o prism.o
	@echo "Archiving $@..."
	@$(AR) rcs $@ $^

tinyQV-sd.a: sdcard/sdcard.o fatfs/ff.o fatfs/ffsystem.o fatfs/ffunicode.o
	@echo "Archiving $@..."
	@$(AR) rcs $@ $^
