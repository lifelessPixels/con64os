
DIRS	:= $(shell find . -mindepth 1 -maxdepth 1 -not -name ".*" -not -path ./Build -prune -type d)
BUILD	:= Build

run: all
	qemu-system-x86_64 -machine type=q35 -bios OVMF.fd -m 4G -drive file=$(BUILD)/disk.img,format=raw -net none -smp 4 -serial stdio -no-reboot -no-shutdown -d cpu_reset,guest_errors,unimp

all:
	@for DIR in $(DIRS); do make install -C $${DIR}/ BUILD_DIR=../$(BUILD); done
	make all -C $(BUILD)
clean:
	@for DIR in $(DIRS); do make clean -C $${DIR}/ BUILD_DIR=.//$(BUILD); done
	make clean -C $(BUILD)
	
.PHONY: all clean