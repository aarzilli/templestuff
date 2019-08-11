// This file replaces function from Kernel/Job.HC and some
// Kernel/SerialDev/Message.HC with custom versions that work with lilith.

U0 lilith_TaskRstAwaitingMsg(CTask *task=NULL)
{//Pop-ups get parent messages so wake-up our pop-ups if we got a msg.
  if (!task) task=Fs;
  do {
    if (TaskValidate(task)) {
      LBtr(&task->task_flags,TASKf_AWAITING_MSG);
      LilithSignalEnqueuedTask(task);
    } else
      break;
  } while (task=task->popup_task);
}

LilithReplaceSyscall("TaskRstAwaitingMsg", &lilith_TaskRstAwaitingMsg);

CJob *lilith_TaskExe(CTask *srv,CTask *master,U8 *data,I64 flags)
{//Queues a request to compile and execute src code text.
  CJob *res;
  if (!data || !TaskValidate(srv) || master && !TaskValidate(master) ||
	srv->popup_task && !Bt(&srv->task_flags,TASKf_FILTER_INPUT))
    return NULL;
  res=ACAlloc(sizeof(CJob));
  res->master_task=master;
  res->job_code=JOBT_EXE_STR;
  res->flags=flags;
  res->aux_str=AStrNew(data);
  res->ctrl=&srv->srv_ctrl;

  while (LBts(&srv->srv_ctrl.flags,JOBCf_LOCKED))
    PAUSE
  if (!TaskValidate(srv)) {
    LBtr(&srv->srv_ctrl.flags,JOBCf_LOCKED);
    JobDel(res);
    return NULL;
  } else {
    LBtr(&srv->task_flags,TASKf_IDLE);
    QueIns(res,srv->srv_ctrl.last_waiting);
    TaskRstAwaitingMsg(srv);
    if (Bt(&flags,JOBf_WAKE_MASTER)) {
      Suspend(master);
    }
  }
  return res;
}

LilithReplaceSyscall("TaskExe", &lilith_TaskExe);

CJob *lilith_TaskText(CTask *srv,CTask *master,U8 *data,I64 flags)
{//Post StdIn text to servant task. Tell who the master task is.
  CJob *res;
  CTask *task;
  if (!data || !TaskValidate(srv) || master && !TaskValidate(master) ||
	srv->popup_task && !Bt(&srv->task_flags,TASKf_FILTER_INPUT))
    return NULL;
  res=ACAlloc(sizeof(CJob));
  res->master_task=master; //in case somebody cares
  res->job_code=JOBT_TEXT_INPUT;
  res->flags=flags;
  res->aux_str=AStrNew(data);

  LilithLockTask(srv);
  task=srv->last_input_filter_task;
  if (Bt(&flags,JOBf_HIGHEST_PRIORITY) || task==srv) {
    if (task!=srv)
      TaskWait(srv);
    task=Spawn(&InputFilterTask,NULL,"Input Filter",,srv);
    task->next_input_filter_task=srv->next_input_filter_task;
    task->last_input_filter_task=srv;
    srv->next_input_filter_task=task;
    task->next_input_filter_task->last_input_filter_task=task;
  } else {
    task=srv->next_input_filter_task;
  }
  LilithUnlockTask(srv);
  res->ctrl=&task->srv_ctrl;
  while (LBts(&task->srv_ctrl.flags,JOBCf_LOCKED))
    PAUSE
  if (!TaskValidate(task)) {
    JobDel(res);
    res=NULL;
  } else {
    LBtr(&task->task_flags,TASKf_IDLE);
    TaskRstAwaitingMsg(task);
    QueIns(res,task->srv_ctrl.last_waiting);
    LBtr(&task->srv_ctrl.flags,JOBCf_LOCKED);
  }
  return res;
}

LilithReplaceSyscall("TaskText", &lilith_TaskText);

CJob *lilith_TaskMsg(CTask *_srv,CTask *master,
	I64 msg_code,I64 arg1,I64 arg2,I64 flags)
{//Post message to servant task.  Tell who the master task is.
//See $LK,"flags",A="MN:JOBf_WAKE_MASTER"$ and $LK,"msg_code",A="MN:MSG_CMD"$.
  CJob *tmpc1,*tmpc;
  CTask *srv=_srv;
  if (!TaskValidate(srv) || master && !TaskValidate(master)||
	srv->popup_task && !Bt(&srv->task_flags,TASKf_FILTER_INPUT))
    return NULL;
  tmpc=ACAlloc(sizeof(CJob));
  tmpc->master_task=master;
  tmpc->job_code=JOBT_MSG;
  tmpc->msg_code=AbsI64(msg_code); //negative means do a down and up
  tmpc->aux1=arg1;
  tmpc->aux2=arg2;
  tmpc->flags=flags;
  if (Bt(&sys_semas[SEMA_RECORD_MACRO],0) &&
	srv!=sys_macro_task && msg_code==MSG_KEY_DOWN) {
    tmpc1=AMAllocIdent(tmpc);
    QueIns(tmpc1,sys_macro_head.last);
  }
  LilithLockTask(srv);
  while (Bt(&srv->task_flags,TASKf_FILTER_INPUT) &&
	!Bt(&flags,JOBf_DONT_FILTER)) {
    CTask *srv2=srv->next_input_filter_task;
    LilithUnlockTask(srv);
    srv=srv2;
    LilithLockTask(srv);
  }
  LilithUnlockTask(srv);
  tmpc->ctrl=&srv->srv_ctrl;
  while (LBts(&srv->srv_ctrl.flags,JOBCf_LOCKED))
    PAUSE
  if (!TaskValidate(srv)) {
    JobDel(tmpc);
    tmpc=NULL;
  } else {
    LBtr(&srv->task_flags,TASKf_IDLE);
    TaskRstAwaitingMsg(srv);
    QueIns(tmpc,srv->srv_ctrl.last_waiting);
    LBtr(&srv->srv_ctrl.flags,JOBCf_LOCKED);
  }
  if (msg_code<0) //Down-Up
    lilith_TaskMsg(_srv,master,-msg_code+1,arg1,arg2,flags);
  return tmpc;
}

LilithReplaceSyscall("TaskMsg", &lilith_TaskMsg);

Bool lilith_JobResScan(CJob *rqst=NULL,I64 *_res=NULL)
{//Check rqst complete, return with or without.
  CJobCtrl *ctrl;
  CJob *tmpc,*tmpc1;
  if (!rqst || Bt(&rqst->flags,JOBf_DONE)) {
    if (!rqst || rqst->master_task)
      ctrl=&Fs->srv_ctrl;
    else
      ctrl=rqst->ctrl;
    while (LBts(&ctrl->flags,JOBCf_LOCKED))
      PAUSE
    tmpc1=&ctrl->next_done;
    tmpc=tmpc1->next;
    while (tmpc!=tmpc1) {
      if (!rqst || rqst==tmpc) {
	QueRem(tmpc);
	LBtr(&ctrl->flags,JOBCf_LOCKED);
	if (_res)
	  *_res=tmpc->res;
	JobDel(tmpc);
	return TRUE;
      }
      tmpc=tmpc->next;
    }
    LBtr(&ctrl->flags,JOBCf_LOCKED);
  }
  if (_res)
    *_res=0;
  return FALSE;
}

LilithReplaceSyscall("JobResScan", &lilith_JobResScan);

U0 lilith_TaskWait(CTask *task=NULL,Bool cmd_line_pmt=FALSE)
{//Wait for idle.
  CTask *task1;
  CJob *tmpc1;
  if (!task) task=Fs;
  if (TaskValidate(task)) {
    while (TRUE) {
      task1=task->last_input_filter_task;
      tmpc1=&task1->srv_ctrl.next_waiting;
      if (task1==Fs || !TaskValidate(task1) ||
	    tmpc1==tmpc1->next && Bt(&task1->task_flags,TASKf_IDLE) &&
	    (!cmd_line_pmt || Bt(&task1->task_flags,TASKf_CMD_LINE_PMT)))
	break;
      LilithWaitForIdleTask(task1);
    }
  }
}

LilithReplaceSyscall("TaskWait", &lilith_TaskWait);

#define JOB_DONE	0
#define JOB_CONT	1
#define JOB_EXIT	2

I64 lilith_JobRunOne(I64 run_flags,CJobCtrl *ctrl)
{//Called with ctrl->flags,JOBCf_LOCKED.
  CJob *tmpc=ctrl->next_waiting;
  CTask   *master;
  I64 res,flags=tmpc->flags,old_flags=GetRFlags;
  if (Bt(&flags,JOBf_EXIT_ON_COMPLETE))
    res=JOB_EXIT;
  else
    res=JOB_CONT;
  switch (tmpc->job_code) {
    case JOBT_SPAWN_TASK:
      QueRem(tmpc);
      LBts(&tmpc->flags,JOBf_DISPATCHED);
      LBtr(&ctrl->flags,JOBCf_LOCKED);
      if (tmpc->aux_str)
	tmpc->spawned_task=Spawn(tmpc->addr,tmpc->fun_arg,
	      tmpc->aux_str,,tmpc->aux1,tmpc->aux2,tmpc->flags);
      else
	tmpc->spawned_task=Spawn(tmpc->addr,tmpc->fun_arg,
	      "Unnamed",,tmpc->aux1,tmpc->aux2,tmpc->flags);
      break;
    case JOBT_CALL:
      QueRem(tmpc);
      LBts(&tmpc->flags,JOBf_DISPATCHED);
      LBtr(&ctrl->flags,JOBCf_LOCKED);
      SetRFlags(run_flags);
      LBtr(&Fs->task_flags,TASKf_IDLE);
      try
	      tmpc->res=(*tmpc->addr)(tmpc->fun_arg);
      catch
	Fs->catch_except=TRUE;
      SetRFlags(old_flags);
      break;
    case JOBT_EXE_STR:
      QueRem(tmpc);
      LBts(&tmpc->flags,JOBf_DISPATCHED);
      LBtr(&ctrl->flags,JOBCf_LOCKED);
      SetRFlags(run_flags);
      LBtr(&Fs->task_flags,TASKf_IDLE);
      try
	      tmpc->res=ExePrint("%s",tmpc->aux_str);
      catch
	Fs->catch_except=TRUE;
      SetRFlags(old_flags);
      break;
    default:
      res=JOB_DONE;
  }
  if (res) {
    if (master=tmpc->master_task) {
      if (!Bt(&flags,JOBf_FREE_ON_COMPLETE)) {
	while (LBts(&master->srv_ctrl.flags,JOBCf_LOCKED))
	  PAUSE
	QueIns(tmpc,master->srv_ctrl.last_done);
	LBts(&tmpc->flags,JOBf_DONE);
	LBtr(&master->srv_ctrl.flags,JOBCf_LOCKED);
	SetRFlags(old_flags);
      }
      if (Bt(&flags,JOBf_FOCUS_MASTER) &&
	    !Bt(&master->win_inhibit,WIf_SELF_FOCUS))
	sys_focus_task=master;
      if (Bt(&flags,JOBf_WAKE_MASTER))
	Suspend(master,FALSE);
    }
    if (Bt(&flags,JOBf_FREE_ON_COMPLETE))
      JobDel(tmpc);
    else if (!master) {
      while (LBts(&ctrl->flags,JOBCf_LOCKED))
	PAUSE;
      QueIns(tmpc,ctrl->last_done);
      LBts(&tmpc->flags,JOBf_DONE);
      LBtr(&ctrl->flags,JOBCf_LOCKED);
      SetRFlags(old_flags);
    }
  }
  return res;
}

LilithReplaceSyscall("JobRunOne", &lilith_JobRunOne);

I64 lilith_JobsHndlr(I64 run_flags,CTask *task=NULL)
{//Handle all waiting cmds and return.
  I64 cnt=0,old_flags=GetRFlags;
  if (!task) task=Fs;
  while (TRUE) {
    while (LBts(&task->srv_ctrl.flags,JOBCf_LOCKED))
      PAUSE
    if (task->srv_ctrl.next_waiting!=&task->srv_ctrl)
      switch (lilith_JobRunOne(run_flags,&task->srv_ctrl)) {
	case JOB_CONT:
	  cnt++;
	  break;
	case JOB_EXIT:
	  Exit;
	case JOB_DONE:
	  goto jh_done;
      }
    else
      goto jh_done;
  }
jh_done:
  LBtr(&task->srv_ctrl.flags,JOBCf_LOCKED);
  LilithSignalIdleTask(task);
  SetRFlags(old_flags);
  return cnt;
}

LilithReplaceSyscall("JobsHndlr", &lilith_JobsHndlr);

Bool lilith_Suspend(CTask *task=NULL,Bool state=TRUE)
{//Tell scheduler to skip task.
  Bool res;
  if (!task) task=Fs;
  if (TaskValidate(task))
    res=LBEqu(&task->task_flags,TASKf_SUSPENDED,state);
  else
    res=FALSE;
  return res;
}

LilithReplaceSyscall("Suspend", &lilith_Suspend);

I64 lilith_ScanMsg(I64 *_arg1=NULL,I64 *_arg2=NULL,I64 mask=~1,CTask *task=NULL)
{/*Check for a message of type specified by a one in the mask.
Throw-out messages not in mask.
If no message fit mask, return NULL immediately.
Remove desired message, return $LK,"msg_code",A="MN:MSG_CMD"$.
Note: This delivers messages from parent down to pop-up.
*/
  I64 res,old_flags;
  CJob *tmpc,*tmpc1;
  if (!task) task=Fs;
  old_flags=GetRFlags;
  LilithLockTask(task);
  tmpc1=&task->srv_ctrl.next_waiting;
  LilithUnlockTask(task);
  while (TRUE) {
    if (task==Fs)
      JobsHndlr(old_flags);
    LilithLockTask(task);
    tmpc=tmpc1->next;
    if (tmpc==tmpc1) {
      LilithUnlockTask(task);
      break;
    } else {
      if (tmpc->job_code==JOBT_MSG) {
	QueRem(tmpc);
	LilithUnlockTask(task);
	SetRFlags(old_flags);
	res=tmpc->msg_code;
	if (_arg1)
	  *_arg1=tmpc->aux1;
	if (_arg2)
	  *_arg2=tmpc->aux2;
	JobDel(tmpc);
	if ((res!=MSG_KEY_DOWN || !(tmpc->aux2&SCF_KEY_DESC) ||
	      !Bt(&task->win_inhibit,WIf_SELF_KEY_DESC)) && Bt(&mask,res))
	  goto sm_done;
      }
    }
    SetRFlags(old_flags);
  }
  res=MSG_NULL;
  if (_arg1)
    *_arg1=0;
  if (_arg2)
    *_arg2=0;
  if (task->parent_task&&task->parent_task->popup_task==task) {
    SetRFlags(old_flags);
    return lilith_ScanMsg(_arg1,_arg2,mask,task->parent_task);
  }
sm_done:
  SetRFlags(old_flags);
  return res;
}

LilithReplaceSyscall("ScanMsg", &lilith_ScanMsg);

I64 lilith_GetKey(I64 *_scan_code=NULL,Bool echo=FALSE,Bool raw_cursor=FALSE)
{//Waits for $LK,"MSG_KEY_DOWN",A="MN:MSG_KEY_DOWN"$ message and returns $LK,"ASCII",A="MN:CH_CTRLA"$.
//Sets $LK,"scan_code",A="FI:::/Doc/CharOverview.DD"$.
  //$LK,"ScanKey",A="MN:ScanKey"$() throws away other message types.
  I64 ch,sc;
  Bool cursor_on=FALSE;
  while (!ScanKey(&ch,&sc,FALSE)) {
    if (IsRaw && raw_cursor) {
      if (!cursor_on && ToI64(GetTSC*5/cnts.time_stamp_freq)&1) {
	'�';
	cursor_on=TRUE;
      } else if (cursor_on && !(ToI64(GetTSC*5/cnts.time_stamp_freq)&1)) {
	'' CH_BACKSPACE;
	cursor_on=FALSE;
      }
    }
    LBts(&Fs->task_flags,TASKf_IDLE);
    LBts(&Fs->task_flags,TASKf_AWAITING_MSG);
    LilithWaitForEnqueuedTask(Fs);
    LBtr(&Fs->task_flags,TASKf_IDLE);
  }
  if (IsRaw && raw_cursor && cursor_on)
    '' CH_BACKSPACE;
  if (echo)
    PutKey(ch,sc);
  if (_scan_code) *_scan_code=sc;
  return ch;
}

LilithReplaceSyscall("GetKey", &lilith_GetKey);

U0 lilith_InputFilterTask()
{
  CJob *tmpc,*tmpc1;
  Bool old_filter;
  I64 old_flags=GetRFlags;
  Fs->win_inhibit=WIG_USER_TASK_DFT;
  LBts(&Fs->task_flags,TASKf_INPUT_FILTER_TASK);
  old_filter=LBts(&Fs->last_input_filter_task->task_flags,TASKf_FILTER_INPUT);
  LBEqu(&Fs->task_flags,TASKf_FILTER_INPUT,old_filter);
  LilithWaitForEnqueuedTask(Fs);
  while (TRUE) {
    JobsHndlr(old_flags);
    tmpc1=&Fs->srv_ctrl.next_waiting;
    tmpc=tmpc1->next;
    if (tmpc==tmpc1)
      break;
    else {
      if (tmpc->job_code==JOBT_TEXT_INPUT) {
	QueRem(tmpc);
	SetRFlags(old_flags);
	try
		ExePrint("%s",tmpc->aux_str);
	catch
	  Fs->catch_except=TRUE;
	JobDel(tmpc);
      } else
	break;
    }
  }
  LilithLockTask(Fs);
  Fs->next_input_filter_task->last_input_filter_task=Fs->last_input_filter_task;
  Fs->last_input_filter_task->next_input_filter_task=Fs->next_input_filter_task;
  if (!old_filter)
    LBtr(&Fs->last_input_filter_task->task_flags,TASKf_FILTER_INPUT);
  LilithUnlockTask(Fs);
  SetRFlags(old_flags);
}

LilithReplaceSyscall("InputFilterTask", &lilith_InputFilterTask);
