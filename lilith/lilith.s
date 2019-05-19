	.macro push_registers
	pushq %rbp
	movq %rsp, %rbp
	pushq %rax
	pushq %rbx
	pushq %rcx
	pushq %rdx
	pushq %rsi
	pushq %rdi
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	movq 0x8(%rbp), %rax
	movq %rax, %fs:0x118
	movq 0x0(%rbp), %rax
	movq %rax, %fs:0x158
	.endm
	
	.macro push_registers_except_rax
	pushq %rbp
	movq %rsp, %rbp
	pushq %rbx
	pushq %rcx
	pushq %rdx
	pushq %rsi
	pushq %rdi
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	movq 0x8(%rbp), %rax
	movq %rax, %fs:0x118
	movq 0x0(%rbp), %rax
	movq %rax, %fs:0x158
	.endm
	
	.macro pop_registers
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rdi
	popq %rsi
	popq %rdx
	popq %rcx
	popq %rbx
	popq %rax
	popq %rbp
	.endm
	
	.macro pop_registers_except_rax
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rdi
	popq %rsi
	popq %rdx
	popq %rcx
	popq %rbx
	popq %rbp
	.endm
	
	.text
	.globl putchar_asm_wrapper
	.type putchar_asm_wrapper, @function
putchar_asm_wrapper:
	push_registers
	movq  0x10(%rbp), %rdi
	call putchar_c_wrapper
	pop_registers
	ret $0x8

	.text
	.globl call_templeos3_asm
	.type call_templeos3_asm, @function
call_templeos3_asm:
	push_registers
	push %rcx
	push %rdx
	push %rsi
	call *%rdi
	pop_registers
	ret

	.text
	.globl call_templeos2_asm
	.type call_templeos2_asm, @function
call_templeos2_asm:
	push_registers_except_rax
	push %rdx
	push %rsi
	call *%rdi
	pop_registers_except_rax
	ret

	.text
	.globl drvlock_asm_wrapper
	.type drvlock_asm_wrapper, @function
drvlock_asm_wrapper:
	// don't do anything
	movq $0, %rax
	ret $0x8

	.text
	.globl jobshdnlr_asm_wrapper
	.type jobshdnlr_asm_wrapper, @function
jobshdnlr_asm_wrapper:
	// don't do anything
	movq $0, %rax
	ret $0x8
	
	.text
	.globl templeos_malloc_asm_wrapper
	.type templeos_malloc_asm_wrapper, @function
templeos_malloc_asm_wrapper:
	push_registers_except_rax
	movq 0x10(%rbp), %rdi
	movq 0x18(%rbp), %rsi
	call templeos_malloc_c_wrapper
	pop_registers_except_rax
	ret $0x10
	
	.text
	.globl templeos_free_asm_wrapper
	.type templeos_free_asm_wrapper, @function
templeos_free_asm_wrapper:
	push_registers
	movq 0x10(%rbp), %rdi
	call templeos_free_c_wrapper
	pop_registers
	ret $0x8

	.text
	.globl redseafilefind_asm_wrapper
	.type redseafilefind_asm_wrapper, @function
redseafilefind_asm_wrapper:
	push_registers_except_rax
	movq 0x10(%rbp), %rdi
	movq 0x18(%rbp), %rsi
	movq 0x20(%rbp), %rdx
	movq 0x28(%rbp), %rcx
	movq 0x30(%rbp), %r8
	call redseafilefind_c_wrapper
	pop_registers_except_rax
	ret $0x28

	.text
	.globl redseafileread_asm_wrapper
	.type redseafileread_asm_wrapper, @function
redseafileread_asm_wrapper:
	push_registers_except_rax
	movq 0x10(%rbp), %rdi
	movq 0x18(%rbp), %rsi
	movq 0x20(%rbp), %rdx
	movq 0x28(%rbp), %rcx
	movq 0x30(%rbp), %r8
	call redseafileread_c_wrapper
	pop_registers_except_rax
	ret $0x28

	.text
	.globl systimerread_asm_wrapper
	.type systimerread_asm_wrapper, @function
systimerread_asm_wrapper:
	push_registers_except_rax
	call systimerread_c_wrapper
	pop_registers_except_rax
	ret

