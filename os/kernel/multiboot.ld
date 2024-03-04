ENTRY(_start)

kernel_vma = 0xffffffff80000000;

SECTIONS {
	. = 1M + kernel_vma;

	load_start = .;

	.text BLOCK(4K) : AT(ADDR(.text) - kernel_vma) {
		*(.multiboot)
		*(.text32)
		*(.text)
	}

	load_end = .;

	.bss BLOCK(4K) : AT(ADDR(.bss) - kernel_vma) {
		*(.bss)
	}

	bss_end = .;
}
