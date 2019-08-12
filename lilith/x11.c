char *x11_display = NULL;
bool x11_enabled = false;

Atom atomNETWMName, atomUTF8String;

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
		return;
	}
	
	XChangeProperty(dis, ti->win, atomNETWMName, atomUTF8String, 8, PropModeReplace, ti->t.Fs->task_title, strlen((char *)(ti->t.Fs->task_title)));
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

bool timespec_diff_gt(struct timespec a, struct timespec b, int64_t d) {
	switch (b.tv_sec - a.tv_sec) {
	case 0:
		return (b.tv_nsec - a.tv_nsec) > d;
	case 1:
		return ((1000000000 - a.tv_nsec) + b.tv_nsec) > d;
	default:
		return true;
	}
}

#define X11_KEYCODE_LOW 8
#define X11_KEYCODE_HIGH 255

KeySym *keycode_to_keysym;
int keysyms_per_keycode;
unsigned int num_lock_modifier_mask;
uint8_t *templeos_normal_key_scan_decode_table, *templeos_ctrl_key_scan_decode_table, *templeos_shift_key_scan_decode_table;
uint8_t ascii_to_scancode1[0x7f];

bool is_keypad(KeySym keysym) {
	return (keysym >= 0xff80) && (keysym <= 0xffbd);
}

void keyevent_convert(unsigned int state, unsigned int keycode, bool keyup, uint8_t *pch, uint64_t *pscancode) {
	*pch = 0;
	*pscancode = 0;
	
	if ((keycode < X11_KEYCODE_LOW) || (keycode > X11_KEYCODE_HIGH)) {
		return;
	}
	KeySym keysym0 = keycode_to_keysym[(keycode-X11_KEYCODE_LOW)*keysyms_per_keycode];
	KeySym keysym1 = keycode_to_keysym[(keycode-X11_KEYCODE_LOW)*keysyms_per_keycode + 1];
	KeySym keysym = keysym0;
	if (((state&num_lock_modifier_mask) != 0) && is_keypad(keysym1)) {
		if ((state&ShiftMask) != 0) {
			keysym = keysym1;
		}
	} else if ((state&ShiftMask) != 0) {
		if (keysym1 != 0) {
			keysym = keysym1;
		}
	}
		
	if ((keysym >= 0x20) && (keysym < 0x7f)) {
		*pscancode = ascii_to_scancode1[keysym];
	} else if ((keysym >= XK_F1) && (keysym <= XK_F12)) {
		*pscancode = (keysym - XK_F1) + SC_F1;
	} else {
		switch (keysym) {
		case XK_BackSpace:
			*pscancode = SC_BACKSPACE;
			break;
		case XK_Escape:
			*pscancode = SC_ESC;
			break;
		case XK_Tab:
			*pscancode = SC_TAB;
			break;
		case XK_Linefeed:
		case XK_Return:
			*pscancode = SC_ENTER;
			break;
		case XK_Control_L:
		case XK_Control_R:
			*pscancode = SC_CTRL;
			break;
		case XK_Alt_L:
		case XK_Alt_R:
			*pscancode = SC_ALT;
			break;
		case XK_Caps_Lock:
		case XK_Shift_Lock:
			*pscancode = SC_CAPS;
			break;
		case XK_Num_Lock:
			*pscancode = SC_NUM;
			break;
		case XK_Scroll_Lock:
			*pscancode = SC_SCROLL;
			break;
		case XK_Left:
			*pscancode = SC_CURSOR_LEFT;
			break;
		case XK_Right:
			*pscancode = SC_CURSOR_RIGHT;
			break;
		case XK_Up:
			*pscancode = SC_CURSOR_UP;
			break;
		case XK_Down:
			*pscancode = SC_CURSOR_DOWN;
			break;
		case XK_Page_Up:
			*pscancode = SC_PAGE_UP;
			break;
		case XK_Page_Down:
			*pscancode = SC_PAGE_DOWN;
			break;
		case XK_Home:
			*pscancode = SC_HOME;
			break;
		case XK_End:
			*pscancode = SC_END;
			break;
		case XK_Insert:
			*pscancode = SC_INS;
			break;
		case XK_Delete:
			*pscancode = SC_DELETE;
			break;
		case XK_Pause:
			*pscancode = SC_PAUSE;
			break;
		case XK_Menu:
			*pscancode = SC_GUI;
			break;
		case XK_Print:
			*pscancode = SC_PRTSCRN1;
			// wtf is SC_PRTSCRN2?
			break;
		}
	}
	//TODO: handle other shit
	
	if (state&ShiftMask) {
		*pscancode |= SCF_SHIFT;
	}
	if (state&LockMask) {
		*pscancode |= SCF_CAPS;
	}
	if (state&num_lock_modifier_mask) {
		*pscancode |= SCF_NUM;
	}
	if (state&ControlMask) {
		*pscancode |= SCF_CTRL;
	}
	if (state&Mod1Mask) {
		*pscancode |= SCF_ALT;
	}
	if (state&Mod4Mask) {
		*pscancode |= SCF_MS_L_DOWN;
		*pscancode |= SCF_MS_R_DOWN;
	}
	if (keyup) {
		*pscancode |= SCF_KEY_UP;
	}
	//TODO: SCF_SCROLL lock
	
	uint8_t *table = templeos_normal_key_scan_decode_table;
	if (*pscancode&SCF_CTRL) table = templeos_ctrl_key_scan_decode_table;
	else if (*pscancode&SCF_SHIFT) {
		if (!(*pscancode&SCF_CAPS)) table = templeos_shift_key_scan_decode_table;
	} else {
		if (*pscancode&SCF_CAPS) table = templeos_shift_key_scan_decode_table;
	}
	
	*pch = table[*pscancode & 0x7f];
	*pscancode |= (*pscancode << 32);
}

void find_num_lock_modifier(Display *dis) {
	XModifierKeymap *mm = XGetModifierMapping(dis);
	
	for(int modifier = 0; modifier < 8; ++modifier) {
		for(int i = 0; i < mm->max_keypermod; ++i) {
			KeyCode keycode = mm->modifiermap[modifier*mm->max_keypermod + i];
			KeySym keysym = keycode_to_keysym[(keycode-X11_KEYCODE_LOW)*keysyms_per_keycode];
			if (keysym == 0xff7f) {
				num_lock_modifier_mask = 1<<modifier;
				XFreeModifiermap(mm);
				return;
			}
		}
	}
	
	num_lock_modifier_mask = 0;
	XFreeModifiermap(mm);
}

void init_scancode_tables(void) {
	templeos_normal_key_scan_decode_table = (uint8_t *)kernel_var64_ptr("NORMAL_KEY_SCAN_DECODE_TABLE");
	templeos_ctrl_key_scan_decode_table = (uint8_t *)kernel_var64_ptr("CTRL_KEY_SCAN_DECODE_TABLE");
	templeos_shift_key_scan_decode_table = (uint8_t *)kernel_var64_ptr("SHIFT_KEY_SCAN_DECODE_TABLE");
	
	uint8_t *nksdt = templeos_normal_key_scan_decode_table;
	
	for (int i = 0; i < 0x7f; ++i) {
		ascii_to_scancode1[i] = 0;
	}
	
	for (int i = 0; i < 16*5; ++i) {
		if (((nksdt[i] >= 0x20) && (nksdt[i] < 0x7f)) || (nksdt[i] == '\n')) {
			ascii_to_scancode1[nksdt[i]] = i;
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
	
	atomNETWMName = XInternAtom(dis, "_NET_WM_NAME", False);
	atomUTF8String = XInternAtom(dis, "UTF8_STRING", False);
	
	int ShmCompletionEventType = XShmGetEventBase(dis) + ShmCompletion;
	
	keycode_to_keysym = XGetKeyboardMapping(dis, X11_KEYCODE_LOW, X11_KEYCODE_HIGH-X11_KEYCODE_LOW+1, &keysyms_per_keycode);
	find_num_lock_modifier(dis);
	
	init_scancode_tables();
	
	struct CGrGlbls *gr = (struct CGrGlbls *)templeos_var64_ptr(sys_winmgr_thread.Fs, "gr");
	struct CTextGlbls *text = templeos_var64_ptr(sys_winmgr_thread.Fs, "text");
	
	struct timespec timo;
	timo.tv_sec = 0;
	timo.tv_nsec = 16670000*2; // remove *2 to get 60fps
	
	struct timespec last_redraw;
	struct timespec curt;
	
	memset(&last_redraw, 0, sizeof(last_redraw));
	
	for(;;) {
		XEvent event;
		
		if (!XPending(dis)) {
			nanosleep(&timo, NULL);
		}
		
		clock_gettime(CLOCK_MONOTONIC, &curt);
		
		if (timespec_diff_gt(last_redraw, curt, timo.tv_nsec)) {
			last_redraw = curt;
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
				
				pthread_mutex_unlock(&thread_create_destruct_mutex);
				call_templeos(&sys_winmgr_thread, "GrUpdateTextBG");
				call_templeos(&sys_winmgr_thread, "GrUpdateTextFG");
				
				call_templeos1(&sys_winmgr_thread, "GrUpdateTaskWin", (uint64_t)task);
				pthread_mutex_lock(&thread_create_destruct_mutex);
				
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
			
			XSync(dis, false);
		}
		
		XNextEvent(dis, &event);
		
		//TODO: handle window close
		//TODO: handle keys
		// - MSG_KEY_UP/MSG_KEY_DOWN
		// - TaskMsg(task, 0, MSG_KEY_UP/MSG_KEY_DOWN, ScanCode2Char(arg2), arg2, 0);
		// - arg2 is the scan code
		// presumably key_down gets repeated as long as the key is held down
		
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
		
		case KeyPress:
			//fallthrough
		case KeyRelease: {
				struct templeos_thread_info *ti = find_thread_info_for_window(event.xany.window);
				if (ti != NULL) {
					int64_t msg_code = MSG_KEY_DOWN;
					if (event.type == KeyRelease) {
						msg_code = MSG_KEY_UP;
					}
					uint8_t ch;
					uint64_t scancode;
					keyevent_convert(event.xkey.state, event.xkey.keycode, msg_code == MSG_KEY_UP, &ch, &scancode);
					call_templeos6(&sys_winmgr_thread, "lilith_TaskMsg", (uint64_t)(ti->t.Fs), (uint64_t)(sys_winmgr_thread.Fs), msg_code, ch, scancode, 0);
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
