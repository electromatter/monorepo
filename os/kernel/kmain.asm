	bits 64
	section .text

	global kmain
kmain:
	hlt
	jmp kmain

;console
;drivers
;tasks
;page tables
;elf loader
;processes

%if 0
hello_world:
	db "Hello, world!", 0
	call clear_screen
	mov rdi, hello_world
	call puts
clear_screen:
	push rbp
	mov rbp, rsp

	; zero out vga text buffer
	mov rax, VGA
	mov rdx, LINES
.clear_line:
	mov rcx, COLS
.clear_col:
	mov word [rax], 0x0f00
	add rax, 2
	dec rcx
	jnz .clear_col
	dec rdx
	jnz .clear_line

	; cursor enable and start scanline
	mov dx, 0x3d4
	mov al, 0x0a
	out dx, al
	mov dx, 0x3d5
	mov al, 0x00
	out dx, al

	; cursor end scanline
	mov dx, 0x3d4
	mov al, 0x0b
	out dx, al
	mov dx, 0x3d5
	mov al, 0x0f
	out dx, al

	mov rdi, 0
	mov rsi, 0
	call move_cursor

	pop rbp
	ret

puts:
	push rbp
	mov rbp, rsp

	; write a string to vga starting at the top left
	mov rdx, VGA
	mov rcx, rdi
.putc:
	mov al, [rcx]
	mov [rdx], al
	inc rdx
	mov al, 0x0f
	mov [rdx], al
	inc rdx
	inc rcx
	cmp byte [rcx], 0
	jnz .putc

	mov rax, VGA
	sub rdx, rax
	shr rdx, 1

	; move the cursor to the end of the string
	mov rdi, rdx
	mov rsi, 0
	call move_cursor

	pop rbp
	ret

move_cursor:
	push rbp
	mov rbp, rsp

	; pos = COLS*y + x
	mov rax, rsi
	mov cl, COLS
	mul cl
	add rax, rdi
	mov rcx, rax

	; cursor position low
	mov dx, 0x3d4
	mov al, 0x0f
	out dx, al
	mov dx, 0x3d5
	mov al, cl
	out dx, al

	; cursor position high
	mov dx, 0x3d4
	mov al, 0x0e
	out dx, al
	mov dx, 0x3d5
	mov al, ch
	out dx, al

	pop rbp
	ret
	;     fe
	;      1ff
	; pt4     ff8
	; pt3       7fc
	; pt2         3fe
	; pt1           1ff
	; off              fff

map_page:
	push rbp
	mov rbp, rsp
	; ensure address is not in the architectural hole
	mov rax, rdi
	sar rax, 12 + 9 + 9 + 9 + 9
	add rax, 1
	cmp rax, 1
	ja .hang
	; ensure address is page aligned
	mov rax, rdi
	and rax, 0xfff
	jnz .hang
	; walk page table and allocate missing branches
	mov rbx, rdi
	lea rdx, [rel pt4]
	and rdx, -0x1000
	or rdx, 7
.pt4:
	mov [rel pt1 + 8 * 511], rdx
	invlpg [VPAGE]
	mov rax, rbx
	shr rax, 12 + 9 + 9 + 9
	and rax, 0x1ff
	mov rdx, [VPAGE + rax * 8]
	test rdx, 1
	jnz .pt3
	mov rdx, [rel free_page]
	cmp rdx, 0
	je .hang
	or rdx, 7
	mov [VPAGE + rax * 8], rdx
	mov [rel pt1 + 8 * 511], rdx
	invlpg [VPAGE]
	mov rcx, [VPAGE]
	mov [rel free_page], rcx
	mov [VPAGE + rax * 8], rdx
	mov rax, 0
	mov rcx, 512
	mov rdi, VPAGE
	rep stosq
.pt3:
	mov [rel pt1 + 8 * 511], rdx
	invlpg [VPAGE]
	mov rax, rbx
	shr rax, 12 + 9 + 9
	and rax, 0x1ff
	mov rdx, [VPAGE + rax * 8]
	test rdx, 1
	jnz .pt2
	mov rdx, [rel free_page]
	cmp rdx, 0
	je .hang
	or rdx, 7
	mov [VPAGE + rax * 8], rdx
	mov [rel pt1 + 8 * 511], rdx
	invlpg [VPAGE]
	mov rcx, [VPAGE]
	mov [rel free_page], rcx
	mov rax, 0
	mov rcx, 512
	mov rdi, VPAGE
	rep stosq
.pt2:
	mov [rel pt1 + 8 * 511], rdx
	invlpg [VPAGE]
	mov rax, rbx
	shr rax, 12 + 9
	and rax, 0x1ff
	mov rdx, [VPAGE + rax * 8]
	test rdx, 1
	jnz .pt1
	mov rdx, [rel free_page]
	cmp rdx, 0
	je .hang
	or rdx, 7
	mov [VPAGE + rax * 8], rdx
	mov [rel pt1 + 8 * 511], rdx
	invlpg [VPAGE]
	mov rcx, [VPAGE]
	mov [rel free_page], rcx
	mov rax, 0
	mov rcx, 512
	mov rdi, VPAGE
	rep stosq
.pt1:
	mov [rel pt1 + 8 * 511], rdx
	invlpg [VPAGE]
	mov rax, rbx
	shr rax, 12
	and rax, 0x1ff
	mov [VPAGE + rax * 8], rsi
	invlpg [rbx]
	pop rbp
	ret
.hang:
	cli
	hlt
	jmp .hang
%endif
