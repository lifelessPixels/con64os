
OUT_DIR	:= Bootloader

all:
	make -C x86_64-efi

install: all
	mkdir -p $(BUILD_DIR)/$(OUT_DIR)
	cp dist/bootboot.efi $(BUILD_DIR)/$(OUT_DIR)/

clean:
	make clean -C x86_64-efi
	rm -rf $(BUILD_DIR)/$(OUT_DIR)

.PHONY: test all install clean