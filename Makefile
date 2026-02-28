# makefile for kTTY (formerly krnel) building

TARGET = krnel.bin
ISO = kTTY.iso
SRC = kernel
INC = include
BUILD = build
ISO_DIR = iso

CC = gcc
AS = nasm
LD = gcc
GRUB = grub-mkrescue

CFLAGS = -m32 -ffreestanding -fno-stack-protector -fno-pic -fno-PIE \
         -Wall -Wextra -I$(INC) -nostdlib -O0 -g -mno-sse -mno-sse2
ASFLAGS = -f elf32
LDFLAGS = -m32 -ffreestanding -nostdlib -T boot/linker.ld -z noexecstack -lgcc

C_SOURCES = $(SRC)/kernel.c \
            $(SRC)/memory.c \
            $(SRC)/string.c \
            $(SRC)/vga.c \
            $(SRC)/keyboard.c \
            $(SRC)/ata.c \
            $(SRC)/ext2.c \
            $(SRC)/fs.c \
            $(SRC)/history.c \
            $(SRC)/user.c \
            $(SRC)/process.c \
            $(SRC)/shell.c \
            $(SRC)/kittywrite.c \
            $(SRC)/syscall.c \
            $(SRC)/plugin.c \
            $(SRC)/stackcheck.c \
            $(SRC)/sysfetch.c

ASM_SOURCES = boot/boot.asm

C_OBJECTS = $(patsubst $(SRC)/%.c, $(BUILD)/%.o, $(C_SOURCES))
ASM_OBJECTS = $(patsubst boot/%.asm, $(BUILD)/%.o, $(ASM_SOURCES))
OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

all: $(BUILD)/$(TARGET)

iso: $(BUILD)/$(ISO)

krnel.img:
	dd if=/dev/zero of=$@ bs=1M count=64
	@echo "✓ Created disk image: $@ (64MB)"

disk: krnel.img

run: $(BUILD)/$(ISO) krnel.img
	qemu-system-x86_64 -cdrom "$(BUILD)/$(ISO)" -m 512M -serial stdio \
	-drive file=krnel.img,format=raw,if=ide,index=0,media=disk -boot d

$(BUILD)/$(TARGET): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "✓ Kernel built: $@ ($$(stat -c%s $@) bytes)"

$(BUILD)/$(ISO): $(BUILD)/$(TARGET)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(BUILD)/$(TARGET) $(ISO_DIR)/boot/
	echo 'set timeout=5' > $(ISO_DIR)/boot/grub/grub.cfg
	echo 'set default=0' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'menuentry "kTTY" {' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '    multiboot /boot/krnel.bin' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '    boot' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '}' >> $(ISO_DIR)/boot/grub/grub.cfg
	$(GRUB) -o $@ $(ISO_DIR) 2>/dev/null || true
	@echo "✓ ISO created: $@"

$(BUILD)/%.o: $(SRC)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo " CC $<"

$(BUILD)/%.o: boot/%.asm
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@
	@echo " AS $<"

remove-disk:
	rm -f krnel.img
	@echo "✓ Disk image removed (krnel.img)"

clean:
	rm -rf $(BUILD) $(ISO_DIR) krnel.img
	@echo "✓ Cleaned everything"

.PHONY: all iso run disk clean
