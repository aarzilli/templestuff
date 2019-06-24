// autogenerated file see scripts/syscall_generate.go
extern void asm_syscall_RawPutChar(void);
extern void asm_syscall_DrvLock(void);
extern void asm_syscall_JobsHndlr(void);
extern void asm_syscall_MAlloc(void);
extern void asm_syscall_Free(void);
extern void asm_syscall_RedSeaFileFind(void);
extern void asm_syscall_RedSeaFileRead(void);
extern void asm_syscall_RedSeaFilesFind(void);
extern void asm_syscall_SysTimerRead(void);
extern void asm_syscall_Snd(void);
extern void asm_syscall_MHeapCtrl(void);
void setup_syscall_trampolines(void) {
	trampoline_kernel_patch("RawPutChar", &asm_syscall_RawPutChar);
	trampoline_kernel_patch("DrvLock", &asm_syscall_DrvLock);
	trampoline_kernel_patch("JobsHndlr", &asm_syscall_JobsHndlr);
	trampoline_kernel_patch("_MALLOC", &asm_syscall_MAlloc);
	trampoline_kernel_patch("_FREE", &asm_syscall_Free);
	trampoline_kernel_patch("RedSeaFileFind", &asm_syscall_RedSeaFileFind);
	trampoline_kernel_patch("RedSeaFileRead", &asm_syscall_RedSeaFileRead);
	trampoline_kernel_patch("RedSeaFilesFind", &asm_syscall_RedSeaFilesFind);
	trampoline_kernel_patch("SysTimerRead", &asm_syscall_SysTimerRead);
	trampoline_kernel_patch("Snd", &asm_syscall_Snd);
	trampoline_kernel_patch("_MHEAP_CTRL", &asm_syscall_MHeapCtrl);
}
