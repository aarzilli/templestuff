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

	.include "syscalls.s"
