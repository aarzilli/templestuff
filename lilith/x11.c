char *x11_display = NULL;
bool x11_enabled = false;

void create_window(struct templeos_thread_info *ti, Display *dis, int screen, unsigned long black, unsigned long white, Visual *visual, int depth) {
	ti->window_initialized = true;
	
	int width = 640;
	int height = 480;
	
	ti->win = XCreateSimpleWindow(dis, RootWindow(dis, screen), 10, 10, width, height, 1, black, white);
	XSelectInput(dis, ti->win, ExposureMask | KeyPressMask);
	XMapWindow(dis, ti->win);
	
	ti->image = XShmCreateImage(dis, visual, depth, ZPixmap, NULL, &ti->shminfo, width, height);
	if (ti->image == NULL) {
		ti->window_failed = true;
		fprintf(stderr, "Task %p/%s: could not create shm image\n", ti->t.Fs, ti->t.Fs->task_name);
		return;
	}
	
	ti->shminfo.shmid = shmget(IPC_PRIVATE, ti->image->bytes_per_line * ti->image->height, IPC_CREAT|0777);
	if (ti->shminfo.shmid < 0) {
		ti->window_failed = true;
		fprintf(stderr, "Task %p/%s: could not create shm allocation: %s\n", ti->t.Fs, ti->t.Fs->task_name, strerror(errno));
		return;
	}
	
	ti->shminfo.shmaddr = ti->image->data = shmat(ti->shminfo.shmid, 0, 0);
	ti->shminfo.readOnly = False; 
	
	if (XShmAttach(dis, &ti->shminfo) == 0) {
		ti->window_failed = true;
	}
}

void image_templeos_to_x11(struct CDC *dc, XImage *image) {
	for (int i = 0; i < dc->height; ++i) {
		for (int j = 0; j  < dc->width; ++j) {
			uint8_t idx = dc->body[(i*dc->width_internal) + j];
			uint64_t bgr = dc->palette[idx];
			//TODO: replace with writing directly to the buffer
			XPutPixel(image, j, i, bgr);
		}
	}
}

struct templeos_thread_info *find_thread_info_for_window(Window win) {
	pthread_mutex_lock(&thread_create_destruct_mutex);
	for (struct templeos_thread_info *ti = first_templeos_task; ti != NULL; ti = ti->next) {
		if ((ti->t.Fs->display_flags & DISPLAY_SHOW) == 0) {
			continue;
		}
		if (ti->win == win) {
			pthread_mutex_unlock(&thread_create_destruct_mutex);
			return ti;
		}
	}
	pthread_mutex_unlock(&thread_create_destruct_mutex);
	return NULL;
}

void x11_start(struct templeos_thread sys_winmgr_thread) {
	if (DEBUG_X11) {
		fprintf(stderr, "serving x11\n");
	}
	Display *dis = XOpenDisplay((char *)0);
	int screen = DefaultScreen(dis);
	Visual *visual = DefaultVisual(dis, screen);
	int depth = DefaultDepth(dis, screen);
	GC gc = DefaultGC(dis, screen);
	
	unsigned long white = WhitePixel(dis, screen);
	unsigned long black = BlackPixel(dis, 	screen);
	
	int ShmCompletionEventType = XShmGetEventBase(dis) + ShmCompletion;
	
	struct CGrGlbls *gr = (struct CGrGlbls *)templeos_var64_ptr(sys_winmgr_thread.Fs, "gr");
	
	struct timespec timo;
	timo.tv_sec = 0;
	timo.tv_nsec = 16670000;
	
	for(;;) {
		XEvent event;
		
		if (!XPending(dis)) {
			nanosleep(&timo, NULL);
		}
				
		pthread_mutex_lock(&thread_create_destruct_mutex);
		for (struct templeos_thread_info *ti = first_templeos_task; ti != NULL; ti = ti->next) {
			if ((ti->t.Fs->display_flags & DISPLAY_SHOW) == 0) {
				continue;
			}
			if (ti->window_failed) {
				continue;
			}
			
			if (ti->t.Fs != sys_winmgr_thread.Fs) { // remove this, do all windows
				continue;
			}
			
			struct CTask *task = ti->t.Fs;
			if (!ti->window_initialized) {
				if (DEBUG_X11) {
					fprintf(stderr, "Task %p/%s: creating window top=%ld bottom=%ld left=%ld right=%ld\n", task, task->task_name, task->win_top, task->win_bottom, task->win_left, task->win_right);
				}
				create_window(ti, dis, screen, black, white, visual, depth);
			}
			
			if (ti->window_failed) {
				continue;
			}
			//TODO: need to set the gr.dc2 of each window to a different value...
			
			if (ti->t.Fs == sys_winmgr_thread.Fs) {
				call_templeos(&sys_winmgr_thread, "GrUpdateTextBG");
				call_templeos(&sys_winmgr_thread, "GrUpdateTextFG");
			}
			
			call_templeos1(&sys_winmgr_thread, "GrUpdateTaskWin", (uint64_t)task);
			
			if (!ti->image_used_by_server) {
				image_templeos_to_x11(gr->dc2, ti->image);
				ti->image_used_by_server = true;
				if (XShmPutImage(dis, ti->win, gc, ti->image, 0, 0, 0, 0, ti->image->width, ti->image->height, False) == 0) {
					fprintf(stderr, "Task %p/%s: could not transmit image\n", ti->t.Fs, ti->t.Fs->task_name);
				}
			}
		}
		pthread_mutex_unlock(&thread_create_destruct_mutex);
		
		XNextEvent(dis, &event);
		
		switch (event.type) {
		case Expose:
			if (event.xexpose.count == 0) {
				struct templeos_thread_info *ti = find_thread_info_for_window(event.xany.window);
				if (ti != NULL) {
					ti->image_used_by_server = true;
					if (XShmPutImage(dis, ti->win, gc, ti->image, 0, 0, 0, 0, ti->image->width, ti->image->height, True) == 0) {
						fprintf(stderr, "Task %p/%s: could not transmit image\n", ti->t.Fs, ti->t.Fs->task_name);
					}
				}
			}
			break;
		
		default:
			if (event.type == ShmCompletionEventType) {
				struct templeos_thread_info *ti = find_thread_info_for_window(event.xany.window);
				if (ti != NULL) {
					ti->image_used_by_server = false;
				}
			}
			break;
		}
	}
}

