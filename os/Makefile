CFLAGS=-Wall -Wextra -pedantic -O1 -std=c89
LD=x86_64-linux-gnu-ld
OD=x86_64-linux-gnu-objdump
QEMU=qemu-system-x86_64
GDB=gdb-multiarch

all: test

test:

ktest: kernel/kernel.o
	$(QEMU) -gdb tcp:127.0.0.1:1234 -m 128m -nographic -monitor none -serial none -display curses -kernel $<

kmonitor: kernel/kernel.o
	$(QEMU) -m 128m -nographic -monitor stdio -serial none -display none -kernel $<

kdump: kernel/kernel.o
	$(OD) -D $<

clean:
	rm -rf */*.o

kattach:
	sleep 0.5  # wait for qemu to enter long mode
	$(GDB) -iex 'set arch i386:x86-64:intel' -iex 'target remote 127.0.0.1:1234' -iex 'symbol-file kernel/kernel.o'

.PHONY: all clean test ktest kmonitor kdump kattach

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.asm
	nasm -f elf64 -o $@ $<

crypto/test.o: crypto/chacha20.c
crypto/test.o: crypto/poly1305.c
crypto/test.o: crypto/sha1.c
crypto/test.o: crypto/sha256.c
crypto/test.o: crypto/sha512.c
crypto/test.o: crypto/aes.c
crypto/test.o: crypto/cv25519.c
crypto/test.o: crypto/rsa.c

kernel/kernel.o: kernel/multiboot.ld kernel/multiboot.o kernel/kmain.o
	$(LD) -m elf_x86_64 -o $@ -T $^
