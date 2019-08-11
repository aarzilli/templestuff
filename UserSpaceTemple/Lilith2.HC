// This file replaces function from Kernel/Job.HC and some
// Kernel/SerialDev/Message.HC with custom versions that work with lilith.

U0 lilith_TaskRstAwaitingMsg(CTask *task=NULL)
{//Pop-ups get parent messages so wake-up our pop-ups if we got a msg.
  if (!task) task=Fs;
  do {
    if (TaskValidate(task)) {
      LBtr(&task->task_flags,TASKf_AWAITING_MSG);
      LilithLockTask(task);
      LilithSignal(task);
      LilithUnlockTask(task);
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

  LilithLockTask(srv);
  LBtr(&srv->task_flags,TASKf_IDLE);
  QueIns(res,srv->srv_ctrl.last_waiting);
  TaskRstAwaitingMsg(srv);
  if (Bt(&flags,JOBf_WAKE_MASTER)) {
    Suspend(master);
    Yield;
  }
  LilithUnlockTask(srv);
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
  LilithLockTask(task);
  if (!TaskValidate(task)) {
    JobDel(res);
    res=NULL;
  } else {
    LBtr(&task->task_flags,TASKf_IDLE);
    TaskRstAwaitingMsg(task);
    QueIns(res,task->srv_ctrl.last_waiting);
    LBtr(&task->srv_ctrl.flags,JOBCf_LOCKED);
  }
  LilithUnlockTask(task);
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
  LilithLockTask(srv)
  while (Bt(&srv->task_flags,TASKf_FILTER_INPUT) &&
	!Bt(&flags,JOBf_DONT_FILTER)) {
    CTask *srv2=srv->next_input_filter_task;
    LilithUnlockTask(srv);
    srv=srv2;
    LilithLockTask(srv);
  }
  tmpc->ctrl=&srv->srv_ctrl;
  LilithUnlockTask(srv);
  while (LBts(&srv->srv_ctrl.flags,JOBCf_LOCKED))
    PAUSE
  if (!TaskValidate(srv)) {
    JobDel(tmpc);
    tmpc=NULL;
  } else {
    LilithLockTask(srv);
    LBtr(&srv->task_flags,TASKf_IDLE);
    TaskRstAwaitingMsg(srv);
    QueIns(tmpc,srv->srv_ctrl.last_waiting);
    LBtr(&srv->srv_ctrl.flags,JOBCf_LOCKED);
    LilithSignal(srv);
    LilithUnlockTask(srv);
  }
  if (msg_code<0) //Down-Up
    lilith_TaskMsg(_srv,master,-msg_code+1,arg1,arg2,flags);
  return tmpc;
}

LilithReplaceSyscall("TaskMsg", &lilith_TaskMsg);

//TODO: JobResScan
//TODO: TaskWait (wait on lilith_job_cond until the queue is empty)
//TODO: JobRunOne
//TODO: JobsHndlr (remember to check if the queue is empty when we exit and signal lilith_job_cond if it is)
//TODO: Suspend
//TODO: Yield (check TASKf_AWAITING_MSG in task->task_flags and if it's set wait on lilith_job_cond variable -- also wait if JobResGet is the caller)

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
    Yield;
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
