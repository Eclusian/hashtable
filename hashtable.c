///
/// hashtable.c
///
/// Implementation of hashtable.h.
///

#include "hashtable.h"

#include <string.h>
#include <limits.h>

#include <flog.h>

#define INITIAL_TAB_LEN	(32)
#define LOAD_FACTOR_LIMIT (2.0)

typedef struct bucket_t
{
	void * key;
	void * value;
	struct bucket_t * next;
} bucket;

struct hashtable_t
{
	int pad;		// Whether or not to pad null-terminated keys.
	ssize_t keysize;
	size_t tablelen;
	size_t nitems;
	void * buffer;	// Place to store padded keys during hashing
	bucket ** lookuptable;
};

/* Bucket functions */

///
/// Since they're static, all bucket functions are assumed to be provided valid
/// arguments. Sanitation/validation/verification/authorization/whatever should
/// occur in ht methods before calling bucket_XXXXX().
///

static bucket * new_bucket()
{
	bucket * b = calloc(1, sizeof(bucket));

#ifdef OVERSAFE
	if(b == NULL)
	{
		flog(LL_ERROR, __LOC__": Could not allocate %lu bytes for a new bucket\n", sizeof(bucket));
	}
#endif
	return b;
}

static int bucket_init(bucket * b, const void * key, void * value, size_t keysize)
{
	b->key = malloc(keysize);
#ifdef OVERSAFE
	if(b->key == NULL)
	{
		flog(LL_ERROR, __LOC__": Could not allocate %lu bytes for a new bucket key\n", keysize);
		return 1;
	}
#endif

	memcpy(b->key, key, keysize);
	b->value = value;
	b->next = NULL;
	return 0;
}


/// Return 0 on success, 1 on failure.
static int bucket_put(bucket ** bhandle, const void * key, void * value, size_t keysize, size_t * nitems)
{
	bucket * b = *bhandle;
	if(b == NULL)
	{
		// b is the first element in the linked list.
		*bhandle = new_bucket();
		if(*bhandle == NULL)
		{
			flog(LL_ERROR, __LOC__": bucket_put failed\n");
			return 1;
		}
		bucket_init(*bhandle, key, value, keysize);
		*nitems += 1;
		return 0;
	}
	// Otherwise, search for an empty bucket.
	while(1) {
		if(b->key == NULL)
		{
			bucket_init(b, key, value, keysize);
			return 0;
		}
		if(b->next == NULL)
		{
			b->next = new_bucket();
			if(b->next == NULL)
			{
				flog(LL_ERROR, __LOC__": bucket_put failed\n");
				return 1;
			}
			bucket_init(b->next, key, value, keysize);
			if(b->next->key == NULL)
			{
				// Initialization failed, undo calloc.
				free(b->next);
				b->next = NULL;
			}
			else
			{
				*nitems += 1;
			}
			return 0;
		}
		if(!memcmp(b->next->key, key, keysize))
		{
			b->next->value = value;
			return 0;
		}
		else
		{
			b = b->next;
			continue;
		}
	}
}

/// Return a pointer to a value that matches key, or NULL if key does not exist
static void * bucket_get(bucket ** bhandle, const void * key, ssize_t keysize)
{
	if(*bhandle == NULL) return NULL; // key does not exist
	bucket * b = *bhandle;
	if(!memcmp(b->key, key, keysize))
	{
		return b->value;
	}
	while(b->next != NULL)
	{
		b = b->next;
		if(!memcmp(b->key, key, keysize))
		{
			return b->value;
		}
	}
	// If we got here, the key does not exist.
	return NULL;
}


/// Find a value from its key and remove it from the hash table, but return a
/// pointer to value (which may have been malloc'd by the user).
/// If key is invalid or not found, return NULL.
static void * bucket_remove(bucket ** bhandle, const void * key, ssize_t keysize, size_t * nitems)
{
	if(*bhandle == NULL) return NULL; // key does not exist
	bucket * b = *bhandle;
	void * retval;
	// Test if root bucket matches.
	if(!memcmp(b->key, key, keysize))
	{
		retval = b->value;
		*bhandle = b->next;
		free(b->key);
		free(b);
		*nitems -= 1;
		return retval;
	}
	// Otherwise search for matching bucket.
	while(b->next != NULL)
	{
		if(!memcmp(b->next->key, key, keysize))
		{
			retval = b->next->value;
			b->next = b->next->next;
			free(b->next->key);
			free(b->next);
			*nitems -= 1;
			return retval;
		}
		b = b->next;
	}
	// If we got here, the key does not exist.
	return NULL;
}


static void free_bucket(bucket * b)
{
	if(b == NULL) return;
	free_bucket(b->next);
	free(b->key);
	free(b);
}


static size_t ht_hash(const void * key, size_t keysize);
static bucket ** find_bucket(const void * key, size_t keysize, hashtable * restrict ht)
{
	size_t idx = ht_hash(key, keysize) % ht->tablelen;
	return &(ht->lookuptable[idx]);
}

/* ht functions */

///
/// ht functions ought to sanitize inputs before
/// passing them to bucket functions.
///


/// The hash function
///
/// Assumes key is non-null and keysize != 0.
///
/// djb2 algorithm k=33, sourced from http://www.cse.yorku.ca/~oz/hash.html
static size_t ht_hash(const void * key, size_t keysize)
{
	size_t hash = 5381;
	const unsigned char * str = key;

	for(size_t i=0; (i<keysize); i++)
		hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */

	return hash;
}


// Convert a user-supplied key into something the hashtable expects to deal
// with.
// e.g. padding, updating keysize, etc
static void digest_key(const void ** restrict key, ssize_t * keysize, hashtable * ht)
{
	if(*keysize == -1)
	{
		*keysize = strlen(*key)+1;
	}
	else if(ht->pad)
	{
		// Padding
		memset(ht->buffer, '\0', ht->keysize);
		*key = strcpy(ht->buffer, *key);
	}
}


static hashtable * __new_hashtable(ssize_t keysize, int pad, size_t tablelen)
{
	if(keysize <= 0 && keysize != -1)
	{
		flog(LL_ERROR, __LOC__": keysize of %ld is not allowed\n", keysize);
		return NULL;
	}
	if(0 < keysize && keysize < 4)
	{
		flog(LL_WARNING, __LOC__": keysize of size %ld may result in high "
				"collision rate and poor hashtable performance. Consider "
				"increasing keysize.\n", keysize);
	}

	hashtable * ht = calloc(1, sizeof(hashtable));

#ifdef OVERSAFE
	if(ht == NULL)
	{
		flog(LL_ERROR, __LOC__": Could not allocate %lu bytes for hashtable\n", sizeof(hashtable));
		return NULL;
	}
#endif

	ht->pad = pad && (keysize != -1);
	if(ht->pad)
	{
		ht->buffer = malloc(keysize);
#ifdef OVERSAFE
		if(ht->buffer == NULL)
		{
			flog(LL_ERROR, __LOC__": Could not allocate %lu bytes for hashtable key buffer\n", keysize);
			free(ht);
			return NULL;
		}
#endif
	}
	else
	{
		ht->buffer = NULL;
	}

	ht->keysize = keysize;
	ht->tablelen = tablelen;

	ht->lookuptable = calloc(tablelen, sizeof(bucket *));
#ifdef OVERSAFE
	if(ht->lookuptable == NULL)
	{
		flog(LL_ERROR, __LOC__": Could not allocate %lu bytes for hashtable's lookup table\n", sizeof(bucket *));
		free(ht->buffer);
		free(ht);
		return NULL;
	}
#endif
	return ht;
}


hashtable * new_hashtable(ssize_t keysize, int pad)
{
	return __new_hashtable(keysize, pad, INITIAL_TAB_LEN);
}


static hashtable * rehash(hashtable * ht)
{
	if(UINT_MAX - ht->tablelen <= ht->tablelen)
	{
		flog(LL_WARNING, __LOC__": Attempted to resize hashtable with "
				"lookuptable length %lu. Extending lookuptable further will "
				"result in unsigned integer wrap in ht->tablelen.\n"
				"ht->tablelen=%lu\n"
				"ht->keysize=%ld\n"
				"ht->nitems=%lu\n",
				ht->tablelen, ht->tablelen, ht->keysize, ht->nitems);
		return ht;
	}

	// Initializing a new hashtable
	hashtable * new_ht = __new_hashtable(ht->keysize, ht->pad, ht->tablelen * 2);

	// Copying items from ht to new_ht:

	bucket * b;
	int err = 0;
	for(size_t i=0; i<ht->tablelen; i++)
	{
		b = ht->lookuptable[i];
		while(b != NULL)
		{
			void
				* key = b->key,
				* value = b->value;
			err = ht_put(key, value, &new_ht);
			if(err) break;
			b = b->next;
		}
		if(err) break;
	}
	if(err)
	{
		flog(LL_ERROR, __LOC__": Failed while copying values to new hashtable\n");
		free_hashtable(new_ht);
		return ht;
	}

	// Free old table
	free_hashtable(ht);

	return new_ht;
}

/// Add a new entry to the table, and rehash if necessary.
/// Return 0 on success, 1 on failure.
int ht_put(const void * key, void * value, hashtable ** restrict hthandle)
{
	if(hthandle == NULL)
	{
		flog(LL_ERROR, __LOC__": hthandle is NULL!\n");
		return 1;
	}
	hashtable * ht = *hthandle;
	if(ht == NULL)
	{
		flog(LL_ERROR, __LOC__": ht is NULL!\n");
		return 1;
	}
	if(key == NULL)
	{
		flog(LL_ERROR, __LOC__": key is NULL!\n");
		return 1;
	}

	// Insert new entry
	ssize_t keysize = ht->keysize;
	digest_key(&key, &keysize, ht);
	bucket ** dst = find_bucket(key, keysize, ht);
	int err = bucket_put(dst, key, value, keysize, &ht->nitems);
	if(err)
		return 1;

	float load_factor = (float)(ht->nitems) / (ht->tablelen);
	if(load_factor > LOAD_FACTOR_LIMIT)
	{
		flog(LL_DEBUG, __LOC__": Rehashing table with %lu items and length %lu\n", ht->nitems, ht->tablelen);
		*hthandle = rehash(ht);
	}

	return 0;
}

/// Return a value from its key.
void * ht_get(const void * key, hashtable * restrict ht)
{
	if(key == NULL)
		return NULL;

	ssize_t keysize = ht->keysize;
	digest_key(&key, &keysize, ht);

	bucket ** dst = find_bucket(key, keysize, ht);
	return bucket_get(dst, key, keysize);
}

/// Remove a key, value pair from the table.
/// If key is invalid or not found, return NULL.
void * ht_remove(const void * key, hashtable * restrict ht)
{
	if(ht == NULL)
	{
		flog(LL_ERROR, __LOC__": ht is NULL!\n");
		return NULL;
	}
	if(key == NULL)
		return NULL;

	ssize_t keysize = ht->keysize;
	digest_key(&key, &keysize, ht);
	bucket ** dst = find_bucket(key, keysize, ht);
	return bucket_remove(dst, key, keysize, &(ht->nitems));
}


// Return a malloc'd array of length len of all stored value pointers.
void * ht_values(hashtable * ht, size_t * len)
{
	if(ht == NULL)
	{
		flog(LL_ERROR, __LOC__": ht is NULL!\n");
		return NULL;
	}
	if(len == NULL)
	{
		flog(LL_ERROR, __LOC__": len is NULL!\n");
		return NULL;
	}

	*len = ht->nitems;
	if(len == 0) return NULL;

	void ** arr = calloc(ht->nitems, sizeof(void *));
#ifdef OVERSAFE
	if(ht == NULL)
	{
		flog(LL_ERROR, __LOC__": Could not allocate %lu bytes for arr\n", (ht->nitems*sizeof(void *)));
		return NULL;
	}
#endif
	void ** vptr = arr;
	for(size_t i=0; i<ht->tablelen; i++)
	{
		bucket * b = ht->lookuptable[i];
		while(b != NULL)
		{
			*vptr++ = b->value;
			b = b->next;
		}
	}
	return arr;
}


/// Return true if the table is empty, false otherwise.
int ht_isempty(hashtable * ht)
{
	if(ht == NULL)
		return 1;

	return !(ht->nitems);
}


size_t ht_nitems(hashtable * ht)
{
	if(ht == NULL)
		return 0;

	return ht->nitems;
}


void free_hashtable(hashtable * ht)
{
	if(ht == NULL)
		return;

	if(!ht_isempty(ht)) for(size_t i=0; i<ht->tablelen; i++)
	{
		free_bucket(ht->lookuptable[i]);
	}
	free(ht->lookuptable);
	free(ht->buffer);
	free(ht);
}
