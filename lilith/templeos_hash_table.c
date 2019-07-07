struct thiter thiter_new(struct CTask *task) {
	struct thiter r;
	r.h = task->hash_table;
	r.i = -1;
	r.first = true;
	r.he = NULL;
	return r;
}

void thiter_next(struct thiter *it) {
	if (it->h == NULL) {
		return;
	}
	
	if (it->he != NULL) {
		it->he = it->he->next;
		it->first = false;
		if (it->he != NULL) {
			return;
		}
	}
	
	++(it->i);
	it->first = true;
	
	if (it->i >= it->h->mask+1) {
		it->h = it->h->next;
		it->i = -1;
		it->he = NULL;
		return;
	}

	it->he = it->h->body[it->i];
	if (it->he != NULL) {
		return;
	}
}

bool thiter_valid(struct thiter *it) {
	return (it->h != NULL);
}

#define PRINT_HASH_TABLE_EARLY_EXIT true

void print_templeos_hash_table(FILE *out, struct CTask *task) {
	bool early_exit = false;
	for (struct thiter it = thiter_new(task); thiter_valid(&it); thiter_next(&it)) {
		struct CHashTable *h = it.h;
		int i = it.i;
		bool first = it.first;
		struct CHash *he = it.he;
		
		if (it.i == -1) {
			fprintf(out, "hashtable at %p\n", h);
			fprintf(out, "\tnext %p\n", h->next);
			fprintf(out, "\tmask %lx (size: %lx)\n", h->mask, h->mask+1);
			fprintf(out, "\tlocked_flags %lx\n", h->locked_flags);
			fprintf(out, "\tbody %p\n", h->body);
			continue;
		}
		
		if (he == NULL) {
			continue;
		}
		
		if (first) {
			if (early_exit) {
				break;
			}
			fprintf(out, "\t[%x] element at %p\n", i, he);
		}
		
		fprintf(out, "\t\tnext %p\n", he->next);
		if (DEBUG_REGISTER_ALL_ALLOCATIONS) {
			if (is_templeos_memory((uint64_t)(he->str))) {
				fprintf(out, "\t\tstr %p [%s]\n", he->str, he->str);
			} else {
				fprintf(out, "\t\tstr %p\n", he->str);
				early_exit = PRINT_HASH_TABLE_EARLY_EXIT;
			}
		} else {
			fprintf(out, "\t\tstr %p [%s]\n", he->str, he->str);
		}
		fprintf(out, "\t\ttype 0x%x\n", he->type);
		fprintf(out, "\t\tuse_cnt %d\n", he->use_cnt);
		
		if ((he->type & HTT_EXPORT_SYS_SYM) != 0) {
			struct CHashExport *ex = (struct CHashExport *)he;
			fprintf(out, "\t\tsrc_link %p\n", ex->super.src_link);
			fprintf(out, "\t\tidx %p\n", ex->super.idx);
			fprintf(out, "\t\tdbg_info %p\n", ex->super.dbg_info);
			fprintf(out, "\t\timport_name %p\n", ex->super.import_name);
			fprintf(out, "\t\tie_lst %p\n", ex->super.ie_lst);
			fprintf(out, "\t\tval 0x%lx\n", ex->val);
		} else if ((he->type & HTT_MODULE) != 0) {
			struct CHashGeneric *mod = (struct CHashGeneric *)he;
			fprintf(out, "\t\tuser_data0 0x%lx\n", mod->user_data0);
			fprintf(out, "\t\tuser_data1 0x%lx\n", mod->user_data1);
			fprintf(out, "\t\tuser_data2 0x%lx\n", mod->user_data2);
		} else if ((he->type & HTT_FUN) != 0) {
			struct CHashFun *fn = (struct CHashFun *)he;
			fprintf(out, "\t\tsrc_link %p\n", fn->super.super.src_link);
			fprintf(out, "\t\tidx %p\n", fn->super.super.idx);
			fprintf(out, "\t\tdbg_info %p\n", fn->super.super.dbg_info);
			fprintf(out, "\t\timport_name %p\n", fn->super.super.import_name);
			fprintf(out, "\t\tie_lst %p\n", fn->super.super.ie_lst);
			fprintf(out, "\t\tsize %lx\n", fn->super.size);
			fprintf(out, "\t\tneg_offset %lx\n", fn->super.neg_offset);
			fprintf(out, "\t\tmember_cnt %x\n", fn->super.member_cnt);
			fprintf(out, "\t\tflags %x\n", fn->super.flags);
			fprintf(out, "\t\tmember_lst_and_root %p\n", fn->super.member_lst_and_root);
			fprintf(out, "\t\tmember_class_base_root %p\n", fn->super.member_class_base_root);
			fprintf(out, "\t\tlast_in_member_lst %p\n", fn->super.last_in_member_lst);
			fprintf(out, "\t\tbase_class %p\n", fn->super.base_class);
			fprintf(out, "\t\tfwd_class %p\n", fn->super.fwd_class);
			fprintf(out, "\t\treturn_class %p\n", fn->return_class);
			fprintf(out, "\t\targ_cnt %x\n", fn->arg_cnt);
			fprintf(out, "\t\tpad %x\n", fn->pad);
			fprintf(out, "\t\tused_reg_mask %x\n", fn->used_reg_mask);
			fprintf(out, "\t\tclobbered_reg_mask %x\n", fn->clobbered_reg_mask);
			fprintf(out, "\t\texe_addr %p\n", fn->exe_addr);
			fprintf(out, "\t\text_lst %p\n", fn->ext_lst);
		}
		
		fprintf(out, "\n");
		if ((he->next != NULL) && DEBUG_REGISTER_ALL_ALLOCATIONS && !is_templeos_memory((uint64_t)(he->next))) {
			fprintf(out, "\t\t(out of templeos memory)\n\n");
			it.he = NULL;
		}
	}
}

bool is_hash_type(struct CHash *he, uint64_t type) {
	if (he == NULL) {
		return false;
	}
	return ((he->type&type) == type);
}

uint64_t ghidra_off(uint64_t rip, uint64_t module_base) {
	if (module_base == 0) {
		return 0;
	}
	return rip - module_base + 0x10000020;
}

bool symbolicate_frame(FILE *out, struct CTask *task, uint64_t rip) {
	struct hash_entry_t *e = hash_find_closest_entry_before(&symbols, rip);
	if (e != NULL) {
		if (strcmp(e->key, "SYS_KERNEL_END") != 0) {
			fprintf(out, "\t\tat %s (0x%lx+0x%lx) ghidra=0x%lx\n", e->key, e->val->val, rip - e->val->val, ghidra_off(rip, e->val->module_base));
			return strcmp(e->key, "Panic") == 0;
		}
	}
	
	// couldn't find symbol or it was SYS_KERNEL_END, use templeos symbol table instead
	
	if (task == NULL) {
		return false;
	}
	
	char *bestfn_name = NULL;
	uint64_t bestfn_val = 0;
	struct CHashGeneric *bestmod = NULL;
	
	for (struct thiter it = thiter_new(task); thiter_valid(&it); thiter_next(&it)) {
		char *curfn_name = NULL;
		uint64_t curfn_val = 0;
		if (is_hash_type(it.he, HTT_EXPORT_SYS_SYM|HTF_IMM)) {
			struct CHashExport *ex = (struct CHashExport *)(it.he);
			curfn_name = (char *)(ex->super.super.str);
			curfn_val = ex->val;
		} else if (is_hash_type(it.he, HTT_FUN)) {
			struct CHashFun *fn = (struct CHashFun *)(it.he);
			curfn_name = (char *)(fn->super.super.super.str);
			curfn_val = (uint64_t)(fn->exe_addr);
		} else if (is_hash_type(it.he, HTT_MODULE|HTF_PUBLIC)) {
			struct CHashGeneric *mod = (struct CHashGeneric *)(it.he);
			if (mod->user_data0 > rip) {
				continue;
			}
			if (bestmod == NULL) {
				bestmod = mod;
			} else {
				if (rip - mod->user_data0 < rip - bestmod->user_data0) {
					bestmod = mod;
				}
			}
		}
		
		if (curfn_name != NULL) {
			if (curfn_val > rip) {
				continue;
			}
			if (bestfn_name == NULL) {
				bestfn_name = curfn_name;
				bestfn_val = curfn_val;
			} else {
				if (rip - curfn_val < rip - bestfn_val) {
					bestfn_name = curfn_name;
					bestfn_val = curfn_val;
				}
			}
		}
	}
	
	uint64_t module_base = 0;
	char *module_name = "unknown";
	
	if (bestmod != NULL) {
		module_base = bestmod->user_data0 + 0x20; // user_data0 is the header address
		module_name = (char *)(bestmod->super.str);
	}
	
	if (bestfn_name != NULL) {
		fprintf(out, "\t\tat %s!%s (0x%lx+0x%lx) module_base=0x%lx ghidra=0x%lx\n", module_name, bestfn_name, bestfn_val, rip - bestfn_val, module_base, ghidra_off(rip, module_base));
	}
	
	return false;
}

int print_stack_trace(FILE *out, struct CTask *task, uint64_t rip, uint64_t rbp, uint64_t rsp) {
	int count = 0;
	
	while ((count < 20) && (rbp != 0)) {
		if (STACKTRACE_PRINT_ARGS && (rsp != 0) && (rbp > rsp) && (rbp - rsp < 0x800)) {
			fprintf(out, "\t\t\tl/a:");
			int count = 0;
			for (uint64_t addr = rsp; addr < rbp; addr += 0x8) {
				fprintf(out, " %lx", *((uint64_t *)addr));
				if (((++count % 4) == 0) && (addr + 0x8 < rbp)) {
					fprintf(out, "\n\t\t\t    ");
				}
			}
			fprintf(out, "\n");
		}
	
		fprintf(out, "\trip=0x%lx rbp=0x%lx rsp=0x%lx\n", rip, rbp, rsp);
		
		if (!is_templeos_memory(rip)) {
			++count;
			break;
		}
		
		bool ispanic = symbolicate_frame(out, task, rip);
		
		if (ispanic && (count == 0)) {
			fprintf(out, "\t\t\tPanic Message = [%s]\n", *((char **)(rbp+0x10)));
		}
		
		rip = *((uint64_t *)(rbp+0x8));
		rsp = rbp+0x10;
		
		rbp = *((uint64_t *)rbp);
		++count;
	}
	
	return count;
}

void print_stack_trace_here(void) {
	uint64_t rbp;
	asm("movq %%rbp, %0" : "=r"(rbp));
	
	uint64_t rip = *((uint64_t *)(rbp+0x8));
	
	struct templeos_thread t;
	bool itm = false;
	
	if (is_templeos_memory(rip)) {
		itm = true;
		exit_templeos(&t);
	}

	print_stack_trace(stderr, t.Fs, rip, rbp, 0);
	
	if (itm) {
		enter_templeos(&t);
	}
}

void *templeos_var64_ptr(struct CTask *task, char *name) {
	for (struct thiter it = thiter_new(task); thiter_valid(&it); thiter_next(&it)) {
		if (!is_hash_type(it.he, HTF_PUBLIC|HTT_GLBL_VAR)) {
			continue;
		}
		if (strcmp((char *)(it.he->str), name) != 0) {
			continue;
		}
		struct CHashGlblVar *gv = (struct CHashGlblVar *)(it.he);
		return (void *)(gv->data_addr);
	}
	return NULL;
}
