
SHELL		:= /bin/bash

CXX			:= x86_64-elf-g++
CPP			:= x86_64-elf-cpp
LD			:= x86_64-elf-ld
STRIP		:= x86_64-elf-strip
CXXFLAGS	:= -std=c++20 -ggdb -Wall -Wextra -Werror -mcmodel=large -mno-red-zone -mgeneral-regs-only -ffreestanding -fno-stack-protector -fno-exceptions -fno-rtti -nostdlib -I.
CPPFLAGS	:= -MMD
LDFLAGS		:= -nostdlib -T kernel.ld -z max-page-size=0x1000
STRIPFLAGS	:= -s -K mmio -K fb -K bootboot -K environment -K initstack -K kernelCodeStart -K kernelCodeEnd -K rodataStart -K rodataEnd

SOURCES		:= $(shell find . -name "*.cpp")
OBJECTS		:= $(SOURCES:.cpp=.o)
DEP			:= $(SOURCES:.cpp=.d)
OUTPUT		:= kernel.elf
OUT_DIR		:= RamDisk

install: all
	mkdir -p $(BUILD_DIR)/$(OUT_DIR)
	cp kernel.elf $(BUILD_DIR)/$(OUT_DIR)/kernel

all: $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@
# $(STRIP) $(STRIPFLAGS) $@

-include $(DEP)	

clean:
	rm -f $(OBJECTS)
	rm -f $(DEP)
	rm -f kernel.elf
	rm -f $(BUILD_DIR)/$(OUT_DIR)/kernel

.PHONY: clean