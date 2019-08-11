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
#include "/Lilith2.HC"

CDate compile_start = Now;

Option(OPTf_WARN_PAREN,ON);
Option(OPTf_WARN_DUP_TYPES,ON);
HashTablePurge(adam_task->hash_table);

if (!Bt(&sys_run_level, RLf_RAW)) {
	// SysGrInit equivalent
	text.font = sys_font_std;
	text.aux_font = sys_font_cyrillic;
	text.cols = GR_WIDTH/FONT_WIDTH;
	text.rows = GR_HEIGHT/FONT_HEIGHT;
	text.border_chars[2] (I64)=0x0908070605040302;
	text.border_chars[10](U32)=0x0D0C0B0A;
	Fs->win_left=0;
	Fs->win_right = text.cols-1;
	Fs->win_top=0;
	Fs->win_bottom = text.rows-1;
}

U8 *start_up_directory = StrNew(Fs->cur_dir);

#include "/Adam/MakeAdam"

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
	Reboot;
}

sys_winmgr_task = adam_task; // TODO: this will eventually need to be removed
//sys_winmgr_task->pix_width = 640; // do I need this?
//sys_winmgr_task->pix_height = 480;

DocTermNew;
WinVert(2,10);

{ // from WinMgrTask
	WinHorz(0,TEXT_COLS-1);
	WinVert(0,TEXT_ROWS-1);
	LBts(&Fs->display_flags,DISPLAYf_NO_BORDER);
	LBts(&Fs->display_flags,DISPLAYf_SHOW);
	gr.dc->win_task=Fs;
	Fs->win_inhibit&=~WIF_SELF_CTRLS;
	GrSetUpTables;
}
LBts(&Fs->display_flags,DISPLAYf_SHOW);
RegInit;
LBts(&sys_run_level,RLf_REGISTRY);
if (!ins_reg.registered)
  InsUnreg;

U0 WallPaperSimple(CTask *_task) {
	I64 i;
	_task->text_attr=BLUE<<4+BLUE;
	for (i = 0; i < 15; ++i) {
		TextPrint(Fs, 0, i, (i<<4)+WHITE, "Hello Mr. God");
	}
}

{ // Replacement for WallPaperInit
	TermBttnNew;
	gr.fp_wall_paper = &WallPaperSimple;
}

if (DrvIsWritable)
	DirMk("/Tmp"); //Good to have a Tmp

Option(OPTf_WARN_PAREN,OFF);
Option(OPTf_WARN_DUP_TYPES,OFF);
LBts(&sys_run_level,RLf_HOME);

#help_index ""
#include "~/MakeHome"

/****
//

sys_winmgr_task=Spawn(&WinMgrTask,NULL,"Window Mgr");
Fs->win_inhibit=WIG_TASK_DFT-WIF_SELF_BORDER
	-WIF_SELF_GRAB_SCROLL-WIF_SELF_CTRLS;
LBts(&Fs->display_flags,DISPLAYf_CHILDREN_NOT_ON_TOP);






//After this file, the Adam task enters $LK,"server mode",A="HI:Job"$.
***/
