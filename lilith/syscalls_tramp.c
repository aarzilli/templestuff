// autogenerated file see scripts/syscall_generate.go
extern void asm_syscall_RawPutChar(void);
extern void asm_syscall_DrvLock(void);
extern void asm_syscall_MAlloc(void);
extern void asm_syscall_Free(void);
extern void asm_syscall_RedSeaFileFind(void);
extern void asm_syscall_RedSeaFileRead(void);
extern void asm_syscall_RedSeaFilesFind(void);
extern void asm_syscall_RedSeaFileWrite(void);
extern void asm_syscall_RedSeaMkDir(void);
extern void asm_syscall_SysTimerRead(void);
extern void asm_syscall_Snd(void);
extern void asm_syscall_MHeapCtrl(void);
extern void asm_syscall_MSize(void);
extern void asm_syscall_NowDateTimeStruct(void);
extern void asm_syscall_GetS(void);
extern void asm_syscall_Busy(void);
extern void asm_syscall_TaskDerivedValsUpdate(void);
extern void asm_syscall_Yield(void);
extern void asm_syscall_KbdTypeMatic(void);
extern void asm_syscall_Spawn(void);
extern void asm_syscall_TaskStkNew(void);
extern void asm_syscall_CallStkGrow(void);
extern void asm_lilith_lock_task(void);
extern void asm_lilith_unlock_task(void);
extern void asm_lilith_replace_syscall(void);
void setup_syscall_trampolines(void) {
	trampoline_kernel_patch(NULL, "RawPutChar", &asm_syscall_RawPutChar);
	trampoline_kernel_patch(NULL, "DrvLock", &asm_syscall_DrvLock);
	trampoline_kernel_patch(NULL, "_MALLOC", &asm_syscall_MAlloc);
	trampoline_kernel_patch(NULL, "_FREE", &asm_syscall_Free);
	trampoline_kernel_patch(NULL, "RedSeaFileFind", &asm_syscall_RedSeaFileFind);
	trampoline_kernel_patch(NULL, "RedSeaFileRead", &asm_syscall_RedSeaFileRead);
	trampoline_kernel_patch(NULL, "RedSeaFilesFind", &asm_syscall_RedSeaFilesFind);
	trampoline_kernel_patch(NULL, "RedSeaFileWrite", &asm_syscall_RedSeaFileWrite);
	trampoline_kernel_patch(NULL, "RedSeaMkDir", &asm_syscall_RedSeaMkDir);
	trampoline_kernel_patch(NULL, "SysTimerRead", &asm_syscall_SysTimerRead);
	trampoline_kernel_patch(NULL, "Snd", &asm_syscall_Snd);
	trampoline_kernel_patch(NULL, "_MHEAP_CTRL", &asm_syscall_MHeapCtrl);
	trampoline_kernel_patch(NULL, "_MSIZE", &asm_syscall_MSize);
	trampoline_kernel_patch(NULL, "NowDateTimeStruct", &asm_syscall_NowDateTimeStruct);
	trampoline_kernel_patch(NULL, "GetS", &asm_syscall_GetS);
	trampoline_kernel_patch(NULL, "Busy", &asm_syscall_Busy);
	trampoline_kernel_patch(NULL, "TaskDerivedValsUpdate", &asm_syscall_TaskDerivedValsUpdate);
	trampoline_kernel_patch(NULL, "_YIELD", &asm_syscall_Yield);
	trampoline_kernel_patch(NULL, "KbdTypeMatic", &asm_syscall_KbdTypeMatic);
	trampoline_kernel_patch(NULL, "Spawn", &asm_syscall_Spawn);
	trampoline_kernel_patch(NULL, "TaskStkNew", &asm_syscall_TaskStkNew);
	trampoline_kernel_patch(NULL, "CallStkGrow", &asm_syscall_CallStkGrow);
}
