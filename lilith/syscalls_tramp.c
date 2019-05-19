void setup_syscall_trampolines(void) {
	trampoline_kernel_patch("_MALLOC", &asm_syscall_MAlloc);
	trampoline_kernel_patch("_FREE", &asm_syscall_Free);
	trampoline_kernel_patch("RawPutChar", &asm_syscall_RawPutChar);
	trampoline_kernel_patch("DrvLock", &asm_syscall_DrvLock);
	trampoline_kernel_patch("JobsHndlr", &asm_syscall_JobsHndlr);
	trampoline_kernel_patch("RedSeaFileFind", &asm_syscall_RedSeaFileFind);
	trampoline_kernel_patch("RedSeaFileRead", &asm_syscall_RedSeaFileRead);
	trampoline_kernel_patch("SysTimerRead", &asm_syscall_SysTimerRead);
}
