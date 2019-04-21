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

int hashfn(char *key) {
	//TODO: replace with a hash function that isn't complete shit.
	int r = 0;
	for (char *p = key; *p != 0; ++p) {
		r += (int)(*p);
	}
	return r;
}
