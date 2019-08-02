char *x11_display = NULL;
bool x11_enabled = false;

void x11_start(struct templeos_thread sys_winmgr_thread) {
	if (DEBUG_X11) {
		fprintf(stderr, "serving x11\n");
	}
	Display *dis = XOpenDisplay((char *)0);
   	//int screen = DefaultScreen(dis);
   	
   	/*
   	unsigned long white = WhitePixel(dis, screen);
   	unsigned long black = BlackPixel(dis, 	screen);
   	*/
   	
   	struct timespec timo;
   	
   	timo.tv_sec = 0;
   	timo.tv_nsec = 16670000;
   	
	for(;;) {
		//XEvent event;
		
		if (!XPending(dis)) {
			nanosleep(&timo, NULL);
		}
		
		call_templeos(&sys_winmgr_thread, "GrUpdateTextBG");
		call_templeos(&sys_winmgr_thread, "GrUpdateTextFG");
		
		pthread_mutex_lock(&thread_create_destruct_mutex);
		for (struct templeos_thread_info *ti = first_templeos_task; ti != NULL; ti = ti->next) {
			if ((ti->t.Fs->display_flags & DISPLAY_SHOW) == 0) {
				continue;
			}
			
			struct CTask *task = ti->t.Fs;
			if (!ti->window_initialized) {
				ti->window_initialized = true;
				if (DEBUG_X11) {
					fprintf(stderr, "Task %p/%s: creating window top=%ld bottom=%ld left=%ld right=%ld\n", task, task->task_name, task->win_top, task->win_bottom, task->win_left, task->win_right);
				}
			}
			
			//TODO: need to set the gr.dc2 of each window to a different value...
			call_templeos1(&sys_winmgr_thread, "GrUpdateTaskWin", (uint64_t)task);
		}
		pthread_mutex_unlock(&thread_create_destruct_mutex);
		
		/*
		XNextEvent(dis, &event);
		
		switch (event.type) {
		case Expose:
			if (event.xexpose.count == 0) {
				XFillRectangle(dis, win, DefaultGC(dis, screen), 20, 20, 10, 10);
				XDrawString(dis, win, DefaultGC(dis, screen), 50, 50, "Hello World!", strlen("Hello World!"));
			}
			
		case KeyPress: {
			char text[255];
			KeySym key;
			if (XLookupString(&event.xkey, text, 255, &key, 0) == 1) {
				if (text[0] == 'q') {
					XDestroyWindow(dis,win);
					XCloseDisplay(dis);	
					exit(1);
				}
			}
		}
		}*/
	}
}
