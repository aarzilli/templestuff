	.text
	.globl putchar_asm_wrapper
	.type putchar_asm_wrapper, @function
putchar_asm_wrapper:
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
	
	movq  16(%rbp), %rdi
	call putchar_c_wrapper
	
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
	ret
