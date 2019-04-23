void hash_init(struct hash_t *h, int sz) {
	h->h = (struct hash_entry_t *)calloc(sz, sizeof(struct hash_entry_t));
	h->sz = sz;
}

struct export_t *hash_get(struct hash_t *h, char *key) {
	int idx = hashfn(key) % h->sz;
	struct hash_entry_t *e = &(h->h[idx]);
	if (e->key == NULL) {
		return NULL;
	}
	while (e != NULL) {
		if (strcmp(e->key, key) == 0) {
			return e->val;
		}
		e = e->next;
	}
	return NULL;
}

void hash_put(struct hash_t *h, char *key, struct export_t *val) {
	//TODO: deal with duplicates
	int idx = hashfn(key) % h->sz;
	struct hash_entry_t *e = &(h->h[idx]);
	if (e->key == NULL) {
		e->key = key;
		e->next = NULL;
		e->val = val;
		return;
	}
	struct hash_entry_t *n = malloc(sizeof(struct hash_entry_t));
	n->key = key;
	n->next = e->next;
	n->val = val;
	e->next = n;
	//TODO: rehash at 70% occupancy or something
}

struct hash_entry_t *hash_find_closest_entry_before(struct hash_t *h, uint64_t v) {
	struct hash_entry_t *best = NULL;
	for (int i = 0; i < h->sz; ++i) {
		if (h->h[i].key == NULL) {
			continue;
		}
		for (struct hash_entry_t *e = &(h->h[i]); e != NULL; e = e->next) {
			if (e->val->val > v) {
				continue;
			}
			if (best == NULL) {
				best = e;
			} else {
				uint64_t best_dist = v - best->val->val;
				uint64_t cur_dist = v - e->val->val;
				
				//printf("checking with %s at %lx (%lx %lx %d)\n", e->key, e->val->val, cur_dist, best_dist, cur_dist < best_dist);
				
				if (cur_dist < best_dist) {
					best = e;
				}
			}
		}
	}
	return best;
}

int hashfn(char *key) {
	//TODO: replace with a hash function that isn't complete shit.
	int r = 0;
	for (char *p = key; *p != 0; ++p) {
		r += (int)(*p);
	}
	return r;
}
