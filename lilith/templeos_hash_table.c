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
		if (is_templeos_memory((uint64_t)(he->str))) {
			fprintf(out, "\t\tstr %p [%s]\n", he->str, he->str);
		} else {
			fprintf(out, "\t\tstr %p\n", he->str);
			early_exit = PRINT_HASH_TABLE_EARLY_EXIT;
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
		}
		
		fprintf(out, "\n");
		if ((he->next != NULL) && !is_templeos_memory((uint64_t)(he->next))) {
			fprintf(out, "\t\t(out of templeos memory)\n\n");
			it.he = NULL;
		}
	}
}
