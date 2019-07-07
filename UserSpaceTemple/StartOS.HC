//This is executed by the Adam task at boot.
//See $LK,"Adam Start-up",A="FF:::/Kernel/KMain.HC,\"StartOS"$.


#help_index "Compiler/Directive"
public extern I8i Option(I64i num,I8i val);
Option(0,0); //(0,0)=EchoOff   (0,1)=EchoOn

#include "/Kernel/KernelA.HH"
#include "/Compiler/CompilerA.HH"
#include "/Kernel/KernelB.HH"
#include "/Kernel/KernelC.HH"
#include "/Compiler/CompilerB.HH"

CDate compile_start = Now;

Option(OPTf_WARN_PAREN,ON);
Option(OPTf_WARN_DUP_TYPES,ON);
HashTablePurge(adam_task->hash_table);

U8 *start_up_directory = StrNew(Fs->cur_dir);

#include "/Rei/MakeAdam"

if (Bt(&sys_run_level, RLf_RAW)) {
	/* The following runs a minimal command line */
	Fs->put_doc=Fs->display_doc=DocNew;
	fp_getstr2=&SysGetStr2; // reverts a change made when we initialized DolDoc
	LBts(&Fs->display_flags,DISPLAYf_SHOW);
	LBts(&sys_run_level, RLf_ADAM_SERVER);
	CDateStruct elapsed;
	Date2Struct(&elapsed, Now() - compile_start);
	"Elapsed %d:%02d.%03d\n", elapsed.min, elapsed.sec, elapsed.sec100;
	Cd(start_up_directory);
	UserCmdLine;
} else {
	DocTermNew;
	WinVert(2,10);
	LBts(&Fs->display_flags,DISPLAYf_SHOW);
	RegInit;
	LBts(&sys_run_level,RLf_REGISTRY);
	if (!ins_reg.registered)
	  InsUnreg;
	/*
	WallPaperInit;
	
	if (DrvIsWritable)
		DirMk("/Tmp"); //Good to have a Tmp
	
	Option(OPTf_WARN_PAREN,OFF);
	Option(OPTf_WARN_DUP_TYPES,OFF);
	LBts(&sys_run_level,RLf_HOME);
	*/
	//#help_index ""
	//#include "~/MakeHome"
}

/****
//

sys_winmgr_task=Spawn(&WinMgrTask,NULL,"Window Mgr");
Fs->win_inhibit=WIG_TASK_DFT-WIF_SELF_BORDER
	-WIF_SELF_GRAB_SCROLL-WIF_SELF_CTRLS;
LBts(&Fs->display_flags,DISPLAYf_CHILDREN_NOT_ON_TOP);






//After this file, the Adam task enters $LK,"server mode",A="HI:Job"$.
***/
