char *x11_display = NULL;
bool x11_enabled = false;

void create_window(struct templeos_thread_info *ti, Display *dis, int screen, unsigned long black, unsigned long white, Visual *visual, int depth) {
	ti->window_initialized = true;
	
	int width = 640;
	int height = 480;
	
	ti->win = XCreateSimpleWindow(dis, RootWindow(dis, screen), 10, 10, width, height, 1, black, white);
	//TODO: disable resize?
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
			if (idx >= COLORS_NUM) {
				idx = 0;
			}
			uint64_t bgr = dc->palette[idx];
			uint64_t b = bgr&0xffff;
			uint64_t g = (bgr&0xffff0000)>>16;
			uint64_t r = (bgr&0xffff00000000)>>32;
			bgr = (b>>8) | (g&0xff00) | ((r&0xff00)<<8);
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

void dbg_dc(struct CDC *dc) {
	if (dc->body != NULL) {
		for (int i = 0; i < dc->height; i++) {
			for (int j = 0; j < /*dc->width*/ 120; j++) {
				if (dc->body[(i*dc->width_internal) + j] > 0xf) {
					printf("B"); // isn't this supposed to be only 4 bits?! wtf?
				} else {
					printf("%x", dc->body[(i*dc->width_internal) + j]);
				}
			}
			printf("\n");
		}
	}
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
	struct CTextGlbls *text = templeos_var64_ptr(sys_winmgr_thread.Fs, "text");
	
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
			
			struct CTask *task = ti->t.Fs;
			if (!ti->window_initialized) {
				if (DEBUG_X11) {
					fprintf(stderr, "Task %p/%s: creating window top=%ld bottom=%ld left=%ld right=%ld\n", task, task->task_name, task->win_top, task->win_bottom, task->win_left, task->win_right);
				}
				create_window(ti, dis, screen, black, white, visual, depth);
				if (ti->t.Fs == sys_winmgr_thread.Fs) {
					ti->dc = gr->dc2;
					ti->text_base = gr->text_base;
				} else {
					ti->dc = (struct CDC *)call_templeos4(&sys_winmgr_thread, "DCNew", gr->dc2->width, gr->dc2->height, (uint64_t)sys_winmgr_thread.Fs, 0);
					ti->text_base = malloc_for_templeos(text->rows*text->cols*sizeof(uint32_t), data_heap, true);
				}
			}
			
			if (ti->window_failed) {
				continue;
			}
			
			struct CDC *original_dc = gr->dc2;
			uint32_t *original_text_base = gr->text_base;
			gr->text_base = ti->text_base;
			gr->dc2 = ti->dc;
			
			call_templeos(&sys_winmgr_thread, "GrUpdateTextBG");
			call_templeos(&sys_winmgr_thread, "GrUpdateTextFG");
			
			call_templeos1(&sys_winmgr_thread, "GrUpdateTaskWin", (uint64_t)task);
			
			gr->text_base = original_text_base;
			gr->dc2 = original_dc;
			
			if (!ti->image_used_by_server) {
				event.type = Expose;
				event.xexpose.display = dis;
				event.xexpose.window = ti->win;
				event.xexpose.x = 0;
				event.xexpose.y = 0;
				event.xexpose.width = ti->image->width;
				event.xexpose.height = ti->image->height;
				event.xexpose.count = 0;
				XSendEvent(dis, ti->win, True, ExposureMask, &event);
			}
		}
		pthread_mutex_unlock(&thread_create_destruct_mutex);
		
		XNextEvent(dis, &event);
		
		//TODO: handle window close
		
		switch (event.type) {
		case Expose:
			if (event.xexpose.count == 0) {
				struct templeos_thread_info *ti = find_thread_info_for_window(event.xany.window);
				if (ti != NULL) {
					image_templeos_to_x11(ti->dc, ti->image);
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

