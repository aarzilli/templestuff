asm {
_LILITH_LOCK_TASK::
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	
	NOP
	NOP
	NOP
	NOP
	
	NOP
	NOP
	NOP
	NOP
	RET
	
_LILITH_UNLOCK_TASK::
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	
	NOP
	NOP
	NOP
	NOP
	
	NOP
	NOP
	NOP
	NOP
	RET
}

public _extern _LILITH_LOCK_TASK U0 LilithLockTask(U8 *task=0);
public _extern _LILITH_UNLOCK_TASK U0 LilithUnlockTask(U8 *task=0);
