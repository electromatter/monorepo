MAGIC	equ 0x1badb002
MBALIGN	equ 0x00001
MEMINFO	equ 0x00002
AOUT	equ 0x10000
FLAGS	equ MBALIGN+MEMINFO+AOUT
PIC1	equ 0x20
PIC2	equ 0xa0
KVMA	equ 0xffffffff80000000
VGA	equ 0xffffffff800b8000
LINES	equ 25
COLS	equ 80
VPAGE	equ KVMA + 0x1000 * 511

	extern load_start, load_end, bss_end

	section .multiboot

	align 4
	dd MAGIC
	dd FLAGS
	dd -MAGIC-FLAGS
	dd load_start-KVMA
	dd load_start-KVMA
	dd load_end-KVMA
	dd bss_end-KVMA
	dd _start-KVMA

	bits 32 ; eax ecx edx are scratch registers
	section .text32

	global _start
_start:
	; multiboot magic number
	cmp eax, 0x2badb002
	jnz .hang

	; disable interrupts
	cli

	; direction forward
	cld

	; mask pic interrupts
	mov dx, PIC1+1
	mov al, 0xff
	out dx, al
	mov dx, PIC2+1
	mov al, 0xff
	out dx, al

	; setup the stack with a null frame
	mov esp, kstack_top - KVMA
	mov ebp, esp

	; load null idt
	sub esp, 8
	mov word [esp], 0
	mov dword [esp + 2], 0
	lidt [esp]
	add esp, 8

	; check for cpuid
	pushf
	pop ecx
	mov eax, ecx
	xor eax, 0x00200000
	push eax
	popf
	pushf
	pop eax
	xor eax, ecx
	test eax, 0x00200000
	jz .hang
	pushf
	or dword [esp], 0x00200000
	popf

	push ebx

	; check for extended cpuid
	mov eax, 0x80000000
	cpuid
	cmp eax, 0x80000001
	jb .hang

	; check for long mode
	mov eax, 0x80000001
	cpuid
	test edx, 0x20000000
	jz .hang

	pop ebx

	; kernel code segment
	mov edx, gdt + 8 - KVMA
	mov dword [edx],     0x00000000
	mov dword [edx + 4], 0x00209800
	add edx, 8

	; kernel data segment
	mov dword [edx],     0x00000000
	mov dword [edx + 4], 0x00009200
	add edx, 8

	; user code segment
	mov dword [edx],     0x00000000
	mov dword [edx + 4], 0x0020f800
	add edx, 8

	; user data segment
	mov dword [edx],     0x00000000
	mov dword [edx + 4], 0x0000f200
	add edx, 8

	; load the gdt
	sub esp, 8
	mov word [esp], (gdt_end - gdt - 1)
	mov dword [esp + 2], gdt - KVMA
	lgdt [esp]
	add esp, 8

	; load null ldt
	mov ecx, 0
	lldt cx

	; enable pae and pge
	mov eax, 0xa0
	mov cr4, eax

	; page table pt4 -> pt3
	mov edx, pt4 - KVMA
	mov eax, pt3 - KVMA
	or eax, 7
	mov [edx], eax
	mov [edx + 511 * 8], eax

	; page table pt3 -> pt2
	mov edx, pt3 - KVMA
	mov eax, pt2 - KVMA
	or eax, 7
	mov [edx], eax
	mov [edx + 510 * 8], eax

	; page table pt2 -> pt1
	mov edx, pt2 - KVMA
	mov eax, pt1 - KVMA
	or eax, 7
	mov [edx], eax

	; page table pt1 -> physical
	mov edx, pt1 - KVMA
	mov eax, 7
	mov ecx, 0
.fill_pages:
	mov [edx], eax
	add edx, 8
	add eax, 4096
	inc ecx
	cmp ecx, 2048
	jnz .fill_pages

	; load the pt4
	mov eax, pt4 - KVMA
	mov cr3, eax

	; enable long mode
	mov ecx, 0xc0000080
	rdmsr
	or eax, 0x100
	wrmsr

	; enable protection and paging
	mov eax, cr0
	or eax, 0x80000001
	mov cr0, eax

	; set cs
	jmp 0x08:.longmode - KVMA

.hang:
	cli
	hlt
	jmp .hang

	bits 64
.longmode:
	push _start64
	ret

	bits 64 ; rax rdi rsi rdx rcx r8 r9 r10 r11  are scratch registers
	section .text

	extern kmain

_start64:
	; set ds es fs gs ss
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	mov rsp, kstack_top

	; create a dummy null frame
	push 0
	push 0
	mov rbp, rsp

	; remove low mapping
	mov qword [rel pt4], 0
	mov qword [rel pt3], 0

	; flush the page table
	mov rax, pt4 - KVMA
	mov cr3, rax

	; reload the gdt
	sub rsp, 16
	mov word [rsp], (gdt_end - gdt - 1)
	mov rax, gdt
	mov [rsp + 2], rax
	lgdt [rsp]
	add rsp, 16

	; tss segment
	mov rdx, gdt + 8 * 5
	mov dword [rdx +  0], 0x0000000
	mov dword [rdx +  4], 0x0008900
	mov dword [rdx +  8], 0x0000000
	mov dword [rdx + 12], 0x0000000
	mov ax, (tss_end - tss - 1)
	mov [rdx], ax
	mov rax, tss
	mov [rdx + 2], ax
	shr rax, 16
	mov [rdx + 4], al
	mov [rdx + 7], ah
	shr rax, 16
	mov [rdx + 8], eax

	; tss
	mov rax, istack_top
	mov [rel tss + 0x24], rax
	mov rax, nmistack_top
	mov [rel tss + 0x2c], rax
	mov ax, 8 * 5
	ltr ax

	; fill the idt with interrupt gates
	mov rdx, idt
	mov rcx, 0
.fill_idt:
	mov rax, isr0
	add rax, rcx
	mov [rdx], ax
	shr rax, 16
	mov [rdx + 6], ax
	shr rax, 16
	mov [rdx + 8], eax
	mov ax, 0x0008
	mov [rdx + 2], ax
	mov ax, 0x8e01
	mov [rdx + 4], ax
	mov dword [rdx + 12], 0
	add rdx, 16
	add rcx, 16
	cmp rcx, 256*16
	jne .fill_idt

	; use a separate nmi stack
	mov word [idt + (2*16 + 4)], 0x8e02

	; allow user to int 0x80
	mov word [idt + (0x80*16 + 4)], 0xee02

	; load the idt
	sub rsp, 16
	mov word [rsp], idt_end-idt-1
	mov rax, idt
	mov [rsp + 2], rax
	lidt [rsp]
	add rsp, 16

	; start pic initialization
	mov dx, PIC1
	mov al, 0x11
	out dx, al
	mov dx, 0x80
	mov al, 0x00
	out dx, al
	mov dx, PIC2
	mov al, 0x11
	out dx, al
	mov dx, 0x80
	mov al, 0x00
	out dx, al


	; pic1 offset
	mov dx, PIC1+1
	mov al, 32
	out dx, al
	mov dx, 0x80
	mov al, 0x00
	out dx, al

	; pic2 offset
	mov dx, PIC1+1
	mov al, 40
	out dx, al
	mov dx, 0x80
	mov al, 0x00
	out dx, al

	; pic2 -> pic1
	mov dx, PIC1+1
	mov al, 0x04
	out dx, al
	mov dx, 0x80
	mov al, 0x00
	out dx, al
	mov dx, PIC2+1
	mov al, 0x02
	out dx, al
	mov dx, 0x80
	mov al, 0x00
	out dx, al

	; enable 8086 pic mode
	mov dx, PIC1+1
	mov al, 0x01
	out dx, al
	mov dx, 0x80
	mov al, 0x00
	out dx, al
	mov dx, PIC2+1
	mov al, 0x01
	out dx, al
	mov dx, 0x80
	mov al, 0x00
	out dx, al

	; enable 100hz pit
	mov dx, 0x43
	mov al, 0x36
	out dx, al
	mov dx, 0x40
	mov al, 0x9b
	out dx, al
	mov dx, 0x40
	mov al, 0x2e
	out dx, al

	; unmask pit
	mov dx, PIC1+1
	mov al, 0xfe
	out dx, al
	mov dx, PIC2+1
	mov al, 0xff
	out dx, al

	; free available pages
	mov ecx, [rbx + 44 + KVMA]
	mov edx, [rbx + 48 + KVMA]
.map_loop:
	cmp rcx, 24
	jl .hang
	cmp dword [rdx + 20 + KVMA], 1
	jnz .map_skip
	mov rsi, [rdx + 12 + KVMA]
	mov rdi, [rdx + 4 + KVMA]
	cmp rdi, 0
	jz .map_skip
	mov rax, rdi
	and rax, 0xfff
	jz .map_aligned
	neg rax
	add rax, 0x1000
	cmp rsi, rax
	jb .map_skip
	add rdi, rax
	sub rsi, rax
.map_aligned:
	cmp rsi, 0x1000
	jb .map_skip
	cmp rdi, bss_end - KVMA
	jb .page_skip
	mov rax, rdi
	or rax, 7
	mov [rel pt1 + 8 * 511], rax
	invlpg [VPAGE]
	mov rax, [rel free_page]
	mov [VPAGE], rax
	mov [rel free_page], rdi
.page_skip:
	add rdi, 0x1000
	sub rsi, 0x1000
	jmp .map_aligned
.map_skip:
	mov eax, [rdx + KVMA]
	add eax, 4
	cmp eax, 24
	jb .hang
	add rdx, rax
	sub rcx, rax
	jnz .map_loop

	; enable interrupts
	sti

	; call to kmain
	jmp kmain

.hang:
	cli
	hlt
	jmp .hang

	; frame
	; rdi		+0	+0x00
	; rsi		+8	+0x08
	; rdx		+16	+0x10
	; rcx		+24	+0x18
	; r8		+32	+0x20
	; r9		+40	+0x28
	; r10		+48	+0x30
	; r11		+56	+0x38
	; r12		+64	+0x40
	; r13		+72	+0x48
	; r14		+80	+0x50
	; r15		+88	+0x58
	; rbx		+96	+0x60
	; rax		+104	+0x68
	; rbp		+112	+0x70
	; vector	+120	+0x78
	; err		+128	+0x80
	; rip		+136	+0x88
	; cs		+144	+0x90
	; rflags	+152	+0x98
	; rsp		+160	+0xa0
	; ss		+168	+0xa8
	;		+176	+0xb0

isr:
	push rbp
	mov rbp, rsp
	push rax
	push rbx
	push r15
	push r14
	push r13
	push r12
	push r11
	push r10
	push r9
	push r8
	push rcx
	push rdx
	push rsi
	push rdi
	cld

	mov rdi, [rsp + 120]
	cmp rdi, 32
	jb .noack
	cmp rdi, 40
	jb .ack1
	cmp rdi, 48
	jae .noack
	mov dx, PIC2
	mov al, 0x20
	out dx, al
.ack1:
	mov dx, PIC1
	mov al, 0x20
	out dx, al
.noack:

	mov rdi, rsp
	add qword [rdi], 1

isr_iretq:
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop r8
	pop r9
	pop r10
	pop r11
	pop r12
	pop r13
	pop r14
	pop r15
	pop rbx
	pop rax
	pop rbp
	add rsp, 16
	iretq

	align 16
isr0:  ; #DE - divide by zero
	push 0
	push 0
	jmp isr

	align 16
isr1:  ; #DB - debug
	push 0
	push 1
	jmp isr

	align 16
isr2:  ; #NMI - non-maskable interrupt
	push 0
	push 2
	jmp isr

	align 16
isr3:  ; #BP - breakpoint
	push 0
	push 3
	jmp isr

	align 16
isr4:  ; #OF - overflow
	push 0
	push 4
	jmp isr

	align 16
isr5:  ; #BR - bound range
	push 0
	push 5
	jmp isr

	align 16
isr6:  ; #UD - invalid opcode
	push 0
	push 6
	jmp isr

	align 16
isr7:  ; #NM - device not available
	push 0
	push 7
	jmp isr

	align 16
isr8:  ; #DF - double fault
	;push err
	push 8
	jmp isr

	align 16
isr9:  ; coprocessor segment overrun
	push 0
	push 9
	jmp isr

	align 16
isr10: ; #TS - invalid tss
	;push err
	push 10
	jmp isr

	align 16
isr11:  ; #NP - segment not present
	;push err
	push 11
	jmp isr

	align 16
isr12:  ; #SS - stack
	;push err
	push 12
	jmp isr

	align 16
isr13:  ; #GP - general protection
	;push err
	push 13
	jmp isr

	align 16
isr14:  ; #PF - page-fault
	;push err
	push 14
	jmp isr

	align 16
isr15:
	push 0
	push 15
	jmp isr

	align 16
isr16:  ; #MF - x87 floating-point
	push 0
	push 16
	jmp isr

	align 16
isr17:  ; #AC alignment-check
	;push err
	push 17
	jmp isr

	align 16
isr18:  ; #MC - machine-check
	push 0
	push 18
	jmp isr

	align 16
isr19:  ; #XF - simd floating-point
	push 0
	push 19
	jmp isr

	align 16
isr20:
	push 0
	push 20
	jmp isr

	align 16
isr21:  ; #CP - control-protection
	;push err
	push 21
	jmp isr

	align 16
isr22:
	push 0
	push 22
	jmp isr

	align 16
isr23:
	push 0
	push 23
	jmp isr

	align 16
isr24:
	push 0
	push 24
	jmp isr

	align 16
isr25:
	push 0
	push 25
	jmp isr

	align 16
isr26:
	push 0
	push 26
	jmp isr

	align 16
isr27:
	push 0
	push 27
	jmp isr

	align 16
isr28:  ; #HV - hypervisor
	push 0
	push 28
	jmp isr

	align 16
isr29:
	;push err
	push 29
	jmp isr

	align 16
isr30:  ; #SX - security
	;push err
	push 30
	jmp isr

	align 16
isr31:
	push 0
	push 31
	jmp isr

	align 16
isr32:
	push 0
	push 32
	jmp isr

	align 16
isr33:
	push 0
	push 33
	jmp isr

	align 16
isr34:
	push 0
	push 34
	jmp isr

	align 16
isr35:
	push 0
	push 35
	jmp isr

	align 16
isr36:
	push 0
	push 36
	jmp isr

	align 16
isr37:
	push 0
	push 37
	jmp isr

	align 16
isr38:
	push 0
	push 38
	jmp isr

	align 16
isr39:
	push 0
	push 39
	jmp isr

	align 16
isr40:
	push 0
	push 40
	jmp isr

	align 16
isr41:
	push 0
	push 41
	jmp isr

	align 16
isr42:
	push 0
	push 42
	jmp isr

	align 16
isr43:
	push 0
	push 43
	jmp isr

	align 16
isr44:
	push 0
	push 44
	jmp isr

	align 16
isr45:
	push 0
	push 45
	jmp isr

	align 16
isr46:
	push 0
	push 46
	jmp isr

	align 16
isr47:
	push 0
	push 47
	jmp isr

	align 16
isr48:
	push 0
	push 48
	jmp isr

	align 16
isr49:
	push 0
	push 49
	jmp isr

	align 16
isr50:
	push 0
	push 50
	jmp isr

	align 16
isr51:
	push 0
	push 51
	jmp isr

	align 16
isr52:
	push 0
	push 52
	jmp isr

	align 16
isr53:
	push 0
	push 53
	jmp isr

	align 16
isr54:
	push 0
	push 54
	jmp isr

	align 16
isr55:
	push 0
	push 55
	jmp isr

	align 16
isr56:
	push 0
	push 56
	jmp isr

	align 16
isr57:
	push 0
	push 57
	jmp isr

	align 16
isr58:
	push 0
	push 58
	jmp isr

	align 16
isr59:
	push 0
	push 59
	jmp isr

	align 16
isr60:
	push 0
	push 60
	jmp isr

	align 16
isr61:
	push 0
	push 61
	jmp isr

	align 16
isr62:
	push 0
	push 62
	jmp isr

	align 16
isr63:
	push 0
	push 63
	jmp isr

	align 16
isr64:
	push 0
	push 64
	jmp isr

	align 16
isr65:
	push 0
	push 65
	jmp isr

	align 16
isr66:
	push 0
	push 66
	jmp isr

	align 16
isr67:
	push 0
	push 67
	jmp isr

	align 16
isr68:
	push 0
	push 68
	jmp isr

	align 16
isr69:
	push 0
	push 69
	jmp isr

	align 16
isr70:
	push 0
	push 70
	jmp isr

	align 16
isr71:
	push 0
	push 71
	jmp isr

	align 16
isr72:
	push 0
	push 72
	jmp isr

	align 16
isr73:
	push 0
	push 73
	jmp isr

	align 16
isr74:
	push 0
	push 74
	jmp isr

	align 16
isr75:
	push 0
	push 75
	jmp isr

	align 16
isr76:
	push 0
	push 76
	jmp isr

	align 16
isr77:
	push 0
	push 77
	jmp isr

	align 16
isr78:
	push 0
	push 78
	jmp isr

	align 16
isr79:
	push 0
	push 79
	jmp isr

	align 16
isr80:
	push 0
	push 80
	jmp isr

	align 16
isr81:
	push 0
	push 81
	jmp isr

	align 16
isr82:
	push 0
	push 82
	jmp isr

	align 16
isr83:
	push 0
	push 83
	jmp isr

	align 16
isr84:
	push 0
	push 84
	jmp isr

	align 16
isr85:
	push 0
	push 85
	jmp isr

	align 16
isr86:
	push 0
	push 86
	jmp isr

	align 16
isr87:
	push 0
	push 87
	jmp isr

	align 16
isr88:
	push 0
	push 88
	jmp isr

	align 16
isr89:
	push 0
	push 89
	jmp isr

	align 16
isr90:
	push 0
	push 90
	jmp isr

	align 16
isr91:
	push 0
	push 91
	jmp isr

	align 16
isr92:
	push 0
	push 92
	jmp isr

	align 16
isr93:
	push 0
	push 93
	jmp isr

	align 16
isr94:
	push 0
	push 94
	jmp isr

	align 16
isr95:
	push 0
	push 95
	jmp isr

	align 16
isr96:
	push 0
	push 96
	jmp isr

	align 16
isr97:
	push 0
	push 97
	jmp isr

	align 16
isr98:
	push 0
	push 98
	jmp isr

	align 16
isr99:
	push 0
	push 99
	jmp isr

	align 16
isr100:
	push 0
	push 100
	jmp isr

	align 16
isr101:
	push 0
	push 101
	jmp isr

	align 16
isr102:
	push 0
	push 102
	jmp isr

	align 16
isr103:
	push 0
	push 103
	jmp isr

	align 16
isr104:
	push 0
	push 104
	jmp isr

	align 16
isr105:
	push 0
	push 105
	jmp isr

	align 16
isr106:
	push 0
	push 106
	jmp isr

	align 16
isr107:
	push 0
	push 107
	jmp isr

	align 16
isr108:
	push 0
	push 108
	jmp isr

	align 16
isr109:
	push 0
	push 109
	jmp isr

	align 16
isr110:
	push 0
	push 110
	jmp isr

	align 16
isr111:
	push 0
	push 111
	jmp isr

	align 16
isr112:
	push 0
	push 112
	jmp isr

	align 16
isr113:
	push 0
	push 113
	jmp isr

	align 16
isr114:
	push 0
	push 114
	jmp isr

	align 16
isr115:
	push 0
	push 115
	jmp isr

	align 16
isr116:
	push 0
	push 116
	jmp isr

	align 16
isr117:
	push 0
	push 117
	jmp isr

	align 16
isr118:
	push 0
	push 118
	jmp isr

	align 16
isr119:
	push 0
	push 119
	jmp isr

	align 16
isr120:
	push 0
	push 120
	jmp isr

	align 16
isr121:
	push 0
	push 121
	jmp isr

	align 16
isr122:
	push 0
	push 122
	jmp isr

	align 16
isr123:
	push 0
	push 123
	jmp isr

	align 16
isr124:
	push 0
	push 124
	jmp isr

	align 16
isr125:
	push 0
	push 125
	jmp isr

	align 16
isr126:
	push 0
	push 126
	jmp isr

	align 16
isr127:
	push 0
	push 127
	jmp isr

	align 16
isr128:
	push 0
	push 128
	jmp isr

	align 16
isr129:
	push 0
	push 129
	jmp isr

	align 16
isr130:
	push 0
	push 130
	jmp isr

	align 16
isr131:
	push 0
	push 131
	jmp isr

	align 16
isr132:
	push 0
	push 132
	jmp isr

	align 16
isr133:
	push 0
	push 133
	jmp isr

	align 16
isr134:
	push 0
	push 134
	jmp isr

	align 16
isr135:
	push 0
	push 135
	jmp isr

	align 16
isr136:
	push 0
	push 136
	jmp isr

	align 16
isr137:
	push 0
	push 137
	jmp isr

	align 16
isr138:
	push 0
	push 138
	jmp isr

	align 16
isr139:
	push 0
	push 139
	jmp isr

	align 16
isr140:
	push 0
	push 140
	jmp isr

	align 16
isr141:
	push 0
	push 141
	jmp isr

	align 16
isr142:
	push 0
	push 142
	jmp isr

	align 16
isr143:
	push 0
	push 143
	jmp isr

	align 16
isr144:
	push 0
	push 144
	jmp isr

	align 16
isr145:
	push 0
	push 145
	jmp isr

	align 16
isr146:
	push 0
	push 146
	jmp isr

	align 16
isr147:
	push 0
	push 147
	jmp isr

	align 16
isr148:
	push 0
	push 148
	jmp isr

	align 16
isr149:
	push 0
	push 149
	jmp isr

	align 16
isr150:
	push 0
	push 150
	jmp isr

	align 16
isr151:
	push 0
	push 151
	jmp isr

	align 16
isr152:
	push 0
	push 152
	jmp isr

	align 16
isr153:
	push 0
	push 153
	jmp isr

	align 16
isr154:
	push 0
	push 154
	jmp isr

	align 16
isr155:
	push 0
	push 155
	jmp isr

	align 16
isr156:
	push 0
	push 156
	jmp isr

	align 16
isr157:
	push 0
	push 157
	jmp isr

	align 16
isr158:
	push 0
	push 158
	jmp isr

	align 16
isr159:
	push 0
	push 159
	jmp isr

	align 16
isr160:
	push 0
	push 160
	jmp isr

	align 16
isr161:
	push 0
	push 161
	jmp isr

	align 16
isr162:
	push 0
	push 162
	jmp isr

	align 16
isr163:
	push 0
	push 163
	jmp isr

	align 16
isr164:
	push 0
	push 164
	jmp isr

	align 16
isr165:
	push 0
	push 165
	jmp isr

	align 16
isr166:
	push 0
	push 166
	jmp isr

	align 16
isr167:
	push 0
	push 167
	jmp isr

	align 16
isr168:
	push 0
	push 168
	jmp isr

	align 16
isr169:
	push 0
	push 169
	jmp isr

	align 16
isr170:
	push 0
	push 170
	jmp isr

	align 16
isr171:
	push 0
	push 171
	jmp isr

	align 16
isr172:
	push 0
	push 172
	jmp isr

	align 16
isr173:
	push 0
	push 173
	jmp isr

	align 16
isr174:
	push 0
	push 174
	jmp isr

	align 16
isr175:
	push 0
	push 175
	jmp isr

	align 16
isr176:
	push 0
	push 176
	jmp isr

	align 16
isr177:
	push 0
	push 177
	jmp isr

	align 16
isr178:
	push 0
	push 178
	jmp isr

	align 16
isr179:
	push 0
	push 179
	jmp isr

	align 16
isr180:
	push 0
	push 180
	jmp isr

	align 16
isr181:
	push 0
	push 181
	jmp isr

	align 16
isr182:
	push 0
	push 182
	jmp isr

	align 16
isr183:
	push 0
	push 183
	jmp isr

	align 16
isr184:
	push 0
	push 184
	jmp isr

	align 16
isr185:
	push 0
	push 185
	jmp isr

	align 16
isr186:
	push 0
	push 186
	jmp isr

	align 16
isr187:
	push 0
	push 187
	jmp isr

	align 16
isr188:
	push 0
	push 188
	jmp isr

	align 16
isr189:
	push 0
	push 189
	jmp isr

	align 16
isr190:
	push 0
	push 190
	jmp isr

	align 16
isr191:
	push 0
	push 191
	jmp isr

	align 16
isr192:
	push 0
	push 192
	jmp isr

	align 16
isr193:
	push 0
	push 193
	jmp isr

	align 16
isr194:
	push 0
	push 194
	jmp isr

	align 16
isr195:
	push 0
	push 195
	jmp isr

	align 16
isr196:
	push 0
	push 196
	jmp isr

	align 16
isr197:
	push 0
	push 197
	jmp isr

	align 16
isr198:
	push 0
	push 198
	jmp isr

	align 16
isr199:
	push 0
	push 199
	jmp isr

	align 16
isr200:
	push 0
	push 200
	jmp isr

	align 16
isr201:
	push 0
	push 201
	jmp isr

	align 16
isr202:
	push 0
	push 202
	jmp isr

	align 16
isr203:
	push 0
	push 203
	jmp isr

	align 16
isr204:
	push 0
	push 204
	jmp isr

	align 16
isr205:
	push 0
	push 205
	jmp isr

	align 16
isr206:
	push 0
	push 206
	jmp isr

	align 16
isr207:
	push 0
	push 207
	jmp isr

	align 16
isr208:
	push 0
	push 208
	jmp isr

	align 16
isr209:
	push 0
	push 209
	jmp isr

	align 16
isr210:
	push 0
	push 210
	jmp isr

	align 16
isr211:
	push 0
	push 211
	jmp isr

	align 16
isr212:
	push 0
	push 212
	jmp isr

	align 16
isr213:
	push 0
	push 213
	jmp isr

	align 16
isr214:
	push 0
	push 214
	jmp isr

	align 16
isr215:
	push 0
	push 215
	jmp isr

	align 16
isr216:
	push 0
	push 216
	jmp isr

	align 16
isr217:
	push 0
	push 217
	jmp isr

	align 16
isr218:
	push 0
	push 218
	jmp isr

	align 16
isr219:
	push 0
	push 219
	jmp isr

	align 16
isr220:
	push 0
	push 220
	jmp isr

	align 16
isr221:
	push 0
	push 221
	jmp isr

	align 16
isr222:
	push 0
	push 222
	jmp isr

	align 16
isr223:
	push 0
	push 223
	jmp isr

	align 16
isr224:
	push 0
	push 224
	jmp isr

	align 16
isr225:
	push 0
	push 225
	jmp isr

	align 16
isr226:
	push 0
	push 226
	jmp isr

	align 16
isr227:
	push 0
	push 227
	jmp isr

	align 16
isr228:
	push 0
	push 228
	jmp isr

	align 16
isr229:
	push 0
	push 229
	jmp isr

	align 16
isr230:
	push 0
	push 230
	jmp isr

	align 16
isr231:
	push 0
	push 231
	jmp isr

	align 16
isr232:
	push 0
	push 232
	jmp isr

	align 16
isr233:
	push 0
	push 233
	jmp isr

	align 16
isr234:
	push 0
	push 234
	jmp isr

	align 16
isr235:
	push 0
	push 235
	jmp isr

	align 16
isr236:
	push 0
	push 236
	jmp isr

	align 16
isr237:
	push 0
	push 237
	jmp isr

	align 16
isr238:
	push 0
	push 238
	jmp isr

	align 16
isr239:
	push 0
	push 239
	jmp isr

	align 16
isr240:
	push 0
	push 240
	jmp isr

	align 16
isr241:
	push 0
	push 241
	jmp isr

	align 16
isr242:
	push 0
	push 242
	jmp isr

	align 16
isr243:
	push 0
	push 243
	jmp isr

	align 16
isr244:
	push 0
	push 244
	jmp isr

	align 16
isr245:
	push 0
	push 245
	jmp isr

	align 16
isr246:
	push 0
	push 246
	jmp isr

	align 16
isr247:
	push 0
	push 247
	jmp isr

	align 16
isr248:
	push 0
	push 248
	jmp isr

	align 16
isr249:
	push 0
	push 249
	jmp isr

	align 16
isr250:
	push 0
	push 250
	jmp isr

	align 16
isr251:
	push 0
	push 251
	jmp isr

	align 16
isr252:
	push 0
	push 252
	jmp isr

	align 16
isr253:
	push 0
	push 253
	jmp isr

	align 16
isr254:
	push 0
	push 254
	jmp isr

	align 16
isr255:
	push 0
	push 255
	jmp isr

	section .bss

	align 16,resb 1
kstack:
	resb 16*1024
kstack_top:

	align 16,resb 1
istack:
	resb 16*1024
istack_top:

	align 16,resb 1
nmistack:
	resb 16*1024
nmistack_top:

	align 16,resb 1
idt:
	resb 256*16
idt_end:

	align 16,resb 1
gdt:
	resb 7*8
gdt_end:

	align 16,resb 1
tss:
	resb 72
tss_end:

	align 4096,resb 1
pt4:
	resb 4096

	align 4096,resb 1
pt3:
	resb 4096

	align 4096,resb 1
pt2:
	resb 4096

	align 4096,resb 1
pt1:
	resb 4096

	align 8,resb 1
free_page:
	resb 8
