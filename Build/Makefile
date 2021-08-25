
SHELL 	:= /bin/bash
DISK	:= disk.img
MOUNT	:= Mount

all: $(DISK) $(MOUNT)
	losetup -P /dev/loop5 $(DISK)
	mount /dev/loop5p1 $(MOUNT)

# prepare ramdisk
	cd RamDisk && tar -cvf ../INITRD *

# install all files
	mkdir -p Mount/EFI/BOOT/
	cp Bootloader/bootboot.efi Mount/EFI/BOOT/bootx64.efi

	mkdir -p Mount/BOOTBOOT/
	cp CONFIG Mount/BOOTBOOT/
	cp INITRD Mount/BOOTBOOT/

	umount $(MOUNT)
	losetup -d /dev/loop5

clean:
	rm -f $(DISK)
	rm -f INITRD

$(DISK):
	dd if=/dev/zero of=$(DISK) bs=512 count=65536
	echo -e "o\ny\nn\n\n\n+16M\nef00\nw\ny\n" | gdisk $(DISK)
	losetup -P /dev/loop5 $(DISK)
	mkdosfs /dev/loop5p1
	losetup -d /dev/loop5

$(MOUNT):
	mkdir -p $(MOUNT)
	rm -rf $(MOUNT)/*

.PHONY: $(MOUNT)