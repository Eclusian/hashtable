/* hashtable.c */

#include "hashtable.h"

#include <string.h>
#include <limits.h>

#define INITIAL_TAB_LEN	(32)
#define LOAD_FACTOR_LIMIT (2.0)

struct entry_t
{
	void *key;
	void *value;
	struct entry_t *next;
};


struct hashtable_t
{
	enum hashtable_flags flags;
	/* Size of a key. Ignored if the HT_VSIZE flag is set. */
	size_t keysize;
	/* Current length of lookuptable. */
	size_t tablelen;
	/* Current number of entries in the table. */
	size_t nitems;
	/* Place to store padded keys during hashing. */
	void *buffer;
	/* The lookup table. */
	struct entry_t **lookuptable;
	/*
	 * Cached array of stored values to return from repeated calls to
	 * ht_values(). Invalidated anytime the table is updated.
	 */
	void **valarr;
};

static void invalidate_valarr(struct hashtable_t *ht)
{
	if(ht->valarr != NULL) {
		free(ht->valarr);
		ht->valarr = NULL;
	}
}

/* Bucket functions */

/*
 * Since they're static, all bucket functions are assumed to be provided valid
 * arguments. Sanitation/validation/verification/authorization/whatever should
 * occur in user-exposed ht methods before calling bucket functions.
 */

/*
 * Create a new entry with the given @key and @value.
 */
static struct entry_t *new_entry(const void *key, void *value, size_t keysize)
{
	struct entry_t *entry = malloc(sizeof(struct entry_t));
	entry->key = malloc(keysize);
	if(key != NULL) {
		memcpy(entry->key, key, keysize);
		entry->value = value;
	}
	entry->next = NULL;
	return entry;
}

/*
 * Frees an entry and its key (but not entry->next).
 */
static void free_entry(struct entry_t *entry)
{
	free(entry->key);
	free(entry);
}

/*
 * Frees every entry in a bucket.
 */
static void free_bucket(struct entry_t *entry)
{
	if(entry == NULL) return;
	free_bucket(entry->next);
	free(entry->key);
	free(entry);
}

/*
 * Frees every entry in a bucket, as well as the entries' value pointer.
 */
static void destroy_bucket(struct entry_t *entry)
{
	if(entry == NULL) return;
	destroy_bucket(entry->next);
	free(entry->key);
	free(entry->value);
	free(entry);
}

/* 
 * Searches a bucket for an entry matching @key. If a match is found,
 * the returned pointer is the struct entry_t *e for which e->next points to
 * the match. If no match is found, the returned pointer is the final
 * entry in the bucket, for which e->next == NULL.
 * Note that this function will never check the first entry of a bucket for
 * a match, since there is no previous entry that can point to it.
 */
static struct entry_t *get_before_entry_from_bucket(struct entry_t *entry,
							const void *key,
							size_t keysize)
{
	while(entry->next != NULL)
		if(!memcmp(entry->next->key, key, keysize)) return entry;
		else entry = entry->next;
	return entry;
}

/*
 * Return a pointer to an entry that matches key, or NULL if key does not
 * exist.
 */
static struct entry_t *get_entry_from_bucket(struct entry_t **bucket,
						const void *key,
						size_t keysize)
{
	struct entry_t *entry = *bucket;
	if(entry == NULL) return NULL;
	do {
		if(!memcmp(entry->key, key, keysize)) return entry;
		entry = entry->next;
	} while(entry != NULL);
	return NULL;
}

/*
 * @bucket - Reference to a bucket (beginning of a linked list of entry_t's)
 *
 * Evaluate the hash of @key and use it to index the hashtable.
 * If an entry is found with a matching key, overwrite its value.
 * If no such entry exists, create a new entry with the given key and value.
 * @ehandle must not be NULL, but *@ehandle may be NULL if @ehandle points to
 * an empty bucket. 
 * If an error occurs, the return value is negative.
 * If an entry matching @key is found and its value successfully overwritten,
 * 0 is returned.
 * If no matching entry is found, but a new entry is created successfully,
 * 1 is returned.
 */
static int add_entry_to_bucket(struct entry_t **bucket, const void *key,
				void *value, size_t keysize)
{
	struct entry_t *entry = *bucket;
	struct entry_t *match = NULL;

	if(entry == NULL) {
		*bucket = new_entry(key, value, keysize);
		return 1;
	}
	/*
	 * Justification for using memcmp():
	 * If HT_VSIZE is unset:
	 *   both entry->key and key are guaranteed to have keysize, no issue.
	 * If HT_VSIZE is set:
	 *   entry->key might be shorter (null-terminated) than keysize:
	 *     - key does not have a '\0' before the keysize'th byte, but
	 *       entry->key does, so they will be reported as different, as
	 *       expected.
	 *   entry->key might be longer than keysize:
	 *     - entry->key will not match key because comparison will fail
	 *       on the null byte.
	 */
	if(!memcmp(entry->key, key, keysize)) {
		entry->value = value;
		return 0;
	}
	match = get_before_entry_from_bucket(entry, key, keysize);
	if(match->next == NULL) {
		match->next = new_entry(key, value, keysize);
		return 1;
	} else {
		match->next->value = value;
		return 0;
	}
}

/*
 * @valueref - A reference to a void * to store the value of the removed entry.
 * Finds the entry for @key and removes it from the bucket, but saves
 * the removed entry's value (which may be a pointer that has been malloc'd by
 * the user) to valueref.
 * If the key is found and successfully removed, 0 is returned and valueref
 * is set to reflect the value of the removed entry.
 * If the key is invalid or is not found, 1 is returned and valueref is set to
 * NULL.
 */
static int remove_entry_from_bucket(struct entry_t **bucket,
					const void *key,
					size_t keysize,
					void **valueref)
{
	if(valueref != NULL) *valueref = NULL;
	struct entry_t *entry = *bucket;
	struct entry_t *match;
	if(entry == NULL)
		return 1;
	if(!memcmp(entry->key, key, keysize)) {
		if(valueref != NULL)
			*valueref = entry->value;
		*bucket = entry->next;
		free_entry(entry);
		return 0;
	}
	match = get_before_entry_from_bucket(entry, key, keysize);
	if(match->next == NULL) {
		return 1;
	} else {
		struct entry_t *tmp = match->next;
		if(valueref != NULL)
			*valueref = tmp->value;
		match->next = match->next->next;
		free_entry(tmp);
		return 0;
	}
}

/*
 * The function used to hash keys. Assumes key is non-null and keysize != 0.
 * djb2 algorithm k=33, sourced from http://www.cse.yorku.ca/~oz/hash.html
 */
static size_t ht_hash(const void *key, size_t keysize)
{
	size_t hash = 5381;
	const unsigned char *str = key;
	for(size_t i=0; (i<keysize); i++)
		hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */

	return hash;
}

/* Find the bucket in the lookup table using the hash of @key. */
static struct entry_t **find_bucket(const void *key, size_t keysize,
					struct hashtable_t *restrict ht)
{
	size_t idx = ht_hash(key, keysize) % ht->tablelen;
	return &(ht->lookuptable[idx]);
}

/* ht functions */

/*
 * ht functions ought to sanitize inputs before
 * passing them to bucket functions.
 */

/*
 * Converts a user-supplied key into something the hashtable expects to deal
 * with.
 * e.g. padding, updating keysize, etc
 */
static void digest_key(const void **restrict key, size_t *keysize,
			struct hashtable_t *ht)
{
	if(ht->flags & HT_VSIZE) {
		*keysize = 1 + strlen(*key);
	} else if(ht->flags & HT_PAD) {
		memset(ht->buffer, '\0', ht->keysize);
		*key = strcpy(ht->buffer, *key);
	}
}

/*
 * Create a new hashtable object.
 */
static struct hashtable_t *new_hashtable__(size_t keysize, int flags,
						size_t tablelen)
{
	struct hashtable_t *ht;
	if(keysize == 0 && (flags & ~HT_VSIZE))
		return NULL;
	
	ht = calloc(1, sizeof(struct hashtable_t));

	if(flags & HT_VSIZE)
		ht->flags |= HT_PAD | HT_VSIZE;
	if(flags & HT_PAD)
		ht->flags |= HT_PAD;

	if(ht->flags & HT_PAD)
		ht->buffer = malloc(keysize);
	else
		ht->buffer = NULL;

	ht->keysize = keysize;
	ht->tablelen = tablelen;
	ht->lookuptable = calloc(tablelen, sizeof(*(ht->lookuptable)));
	ht->valarr = NULL;
	return ht;
}

/*
 * Free all of the memory used by @ht.
 * This does not call free() on entries' value pointers. If this is the desired
 * behavior, see destroy_hashtable().
 */
static void free_hashtable__(struct hashtable_t *ht)
{
	if(ht == NULL) return;
	if(!ht_isempty(&ht)) for(size_t i=0; i<ht->tablelen; i++)
		free_bucket(ht->lookuptable[i]);
	invalidate_valarr(ht);
	free(ht->lookuptable);
	free(ht->buffer);
	free(ht);
}

static void destroy_hashtable__(struct hashtable_t *ht)
{
	if(ht == NULL) return;
	if(!ht_isempty(&ht)) for(size_t i=0; i<ht->tablelen; i++)
		destroy_bucket(ht->lookuptable[i]);
	invalidate_valarr(ht);
	free(ht->lookuptable);
	free(ht->buffer);
	free(ht);
}

/*
 * The user receives a handle to the hashtable rather than a direct pointer,
 * in order to hide realloc-related implementation details.
 */
hashtable *new_hashtable(size_t keysize, int flags)
{
	hashtable *hthandle = malloc(sizeof(hashtable *));
	*hthandle = new_hashtable__(keysize, flags, INITIAL_TAB_LEN);
	return hthandle;
}

/*
 * Rehash @ht. That is, rebuild @ht with a longer tablelen, and free the
 * original @ht while returning the new hashtable.
 */
static struct hashtable_t *rehash(struct hashtable_t *ht)
{
	struct hashtable_t *new_ht;
	struct entry_t *entry;
	void *key, *value;
	if(2*ht->tablelen <= ht->tablelen) return ht;

	new_ht = new_hashtable__(ht->keysize, ht->flags, 2*ht->tablelen);
	for(size_t i=0; i<ht->tablelen; i++) {
		entry = ht->lookuptable[i];
		while(entry != NULL) {
			key = entry->key;
			value = entry->value;
			if(ht_put(key, value, &new_ht) != 0) goto rehash_error;
			entry = entry->next;
		}
	}
	free_hashtable__(ht);
	return new_ht;

rehash_error:
	free_hashtable__(new_ht);
	return ht;
}

/* 
 * Add a new entry to the table, and rehash if necessary.
 * Return 0 on success, 1 on failure.
 */
int ht_put(const void *key, void *value, hashtable *restrict hthandle)
{
	struct hashtable_t *ht;
	struct entry_t **dst_bucket;
	size_t keysize;
	float load_factor;
	int err;
	if(hthandle == NULL || (ht = *hthandle) == NULL || key == NULL)
		return 1;

	keysize = ht->keysize;
	digest_key(&key, &keysize, ht);
	dst_bucket = find_bucket(key, keysize, ht);
	err = add_entry_to_bucket(dst_bucket, key, value, keysize);
	if(err < 0) return 1;
	ht->nitems++;
	invalidate_valarr(ht);

	load_factor = (float)(ht->nitems) / (ht->tablelen);
	if(load_factor > LOAD_FACTOR_LIMIT)
		*hthandle = rehash(ht);

	return 0;
}

/* Return a value from its key. */
void *ht_get(const void *key, hashtable *restrict hthandle)
{
	struct hashtable_t *ht;
	struct entry_t **bucket, *entry;
	if(hthandle == NULL || (ht = *hthandle) == NULL || key == NULL)
		return NULL;

	digest_key(&key, &ht->keysize, ht);

	bucket = find_bucket(key, ht->keysize, ht);
	entry = get_entry_from_bucket(bucket, key, ht->keysize);
	if(entry == NULL) return NULL;
	else return entry->value;
}

/* Returns true if @ht contains the given @key, false otherwise. */
int ht_contains(const void *key, hashtable *restrict hthandle)
{
	struct hashtable_t *ht;
	struct entry_t **bucket;
	struct entry_t *entry;
	if(hthandle == NULL || (ht = *hthandle) == NULL || key == NULL)
		return 0;

	digest_key(&key, &ht->keysize, ht);
	bucket = find_bucket(key, ht->keysize, ht);
	entry = get_entry_from_bucket(bucket, key, ht->keysize);
	if(entry == NULL) return 0;
	else return 1;
}

/*
 * Remove a key, value pair from the table.
 * If @key is invalid or not found, return NULL.
 */
void *ht_remove(const void *key, hashtable *restrict hthandle)
{
	struct hashtable_t *ht;
	void *value = NULL;
	int found;
	struct entry_t **dst;
	if(hthandle == NULL || (ht = *hthandle) == NULL || key == NULL)
		return NULL;

	digest_key(&key, &ht->keysize, ht);
	dst = find_bucket(key, ht->keysize, ht);
	found = !remove_entry_from_bucket(dst, key, ht->keysize, &value);
	if(found) ht->nitems--;
	invalidate_valarr(ht);
	return value;
}


/* 
 * Return an array of the values of all the entries stored in @hthandle.
 * This array must NOT be freed by the user!
 * If @len is not NULL, the length of the returned array (equal to ht_nitems())
 * is written to @len.
 * If the hashtable is empty, return NULL. @len is set to 0 to indicate an
 * empty array.
 */
void *ht_values(hashtable *hthandle, size_t *len)
{
	struct hashtable_t *ht;
	struct entry_t *entry;
	void **vptr;
	if(hthandle == NULL || (ht = *hthandle) == NULL || len == NULL)
		return NULL;

	*len = ht->nitems;
	if(*len == 0) return NULL;
	if(ht->valarr != NULL) return ht->valarr;

	ht->valarr = calloc(ht->nitems, sizeof(void *));
	vptr = ht->valarr;
	for(size_t i=0; i<ht->tablelen; i++) {
		entry = ht->lookuptable[i];
		while(entry != NULL) {
			*vptr++ = entry->value;
			entry = entry->next;
		}
	}
	return ht->valarr;
}

/* Return true if the table is empty, false otherwise. */
int ht_isempty(hashtable *hthandle)
{
	struct hashtable_t *ht;
	if(hthandle == NULL || (ht = *hthandle) == NULL) return 1;
	return !(ht->nitems);
}

/* Return the number of entries currently held by @hthandle. */
size_t ht_nitems(hashtable *hthandle)
{
	struct hashtable_t *ht;
	if(hthandle == NULL || (ht = *hthandle) == NULL) return 0;
	return ht->nitems;
}

/*
 * Free all of the memory used by @hthandle.
 * This does not call free() on entries' value pointers. If this is the desired
 * behavior, see destroy_hashtable().
 */
void free_hashtable(hashtable *hthandle)
{
	if(hthandle == NULL) return;
	free_hashtable__(*hthandle);
	free(hthandle);
}


/*
 * Equivalent to free_hashtable(), except user-defined value pointers are
 * also free()'d.
 * Do not use this function if your hashtable contains ANY pointers to
 * automatic variables!
 */
void destroy_hashtable(hashtable *hthandle)
{
	if(hthandle == NULL) return;
	destroy_hashtable__(*hthandle);
	free(hthandle);
}

