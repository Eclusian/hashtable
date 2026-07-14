///
/// hashtable.h
///
/// Structures and methods used for hashtable.
/// Memory allocation of structures pointed to by value pointers are not
/// managed by the hashtable. Keys-- such as strings-- are allocated within the
/// hashtable itself.
///

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdlib.h>

typedef struct hashtable_t *hashtable;

enum hashtable_flags
{
	/* Whether to pad keys smaller than keysize. */
	HT_PAD = 1<<0,
	/*
	 * Whether to ignore keysize, and instead hash up until a null
	 * byte. If this bit is set, HT_PAD is ignored.
	 */
	HT_VSIZE = 1<<1,
};

/*
 * Return a new empty hashtable where keys have size @keysize.
 * Keys with size larger than @keysize will be truncated to fit into @keysize.
 * Keys smaller than @keysize should not be used unless HT_PAD is set in @flags.
 * If HT_PAD is set in @flags, null-terminated keys smaller than @keysize will
 * be padded with more null bytes before hashing until its size is @keysize.
 * If HT_PAD is not set, then using keys smaller than @keysize will result in
 * undefined behavior.
 * If HT_VSIZE is set in @flags, then HT_PAD and @keysize are ignored, and keys 
 * are hashed until encountering a null byte (useful for string keys).
 */
hashtable *new_hashtable(size_t keysize, int flags);

/*
 * Put a new key, value pair into @ht.
 * An instance of @key is allocated within the table, but only a pointer to
 * @value is stored, so the lifetime of @value should be until it is removed
 * from the table with ht_remove(), or until the hashtable itself is freed.
 * If @key already exists, the corresponding entry is overwritten.
 * Returns 0 on success, 1 on failure.
 */
int ht_put(const void *key, void *value, hashtable *restrict ht);

/*
 * Retrieves a pointer to the value mapped to by @key.
 * If @key is invalid, NULL is returned.
 */
void *ht_get(const void *key, hashtable *restrict ht);

/* Returns true if @ht contains the given @key, false otherwise. */
int ht_contains(const void *key, hashtable *restrict ht);

/*
 * Removes an entry from @ht.
 * If @key is invalid or no matching entry is found, NULL is returned.
 */
void *ht_remove(const void *key, hashtable *restrict ht);

/*
 * Removes an entry from @ht AND calls free() on the entry's value.
 */
void ht_destroy(hashtable *ht, const void *key);

/*
 * Returns an array of all values in @ht, and sets @len to the length of that
 * array. This array need not, and should not, be freed by the user.
 * Sets @len to 0 and returns NULL if @ht is empty. @len may be NULL.
 */
void *ht_values(hashtable *ht, size_t *len);

/* Returns true if @ht contains no entries, false otherwise. */
int ht_isempty(hashtable *ht);

/* Returns the number of elements currently stored in @ht. */
size_t ht_nitems(hashtable *ht);

/*
 * Frees all memory utilized by @ht and stored keys.
 * Hashtable entries are freed by this function, but values pointed to by
 * those entries are NOT freed by this function. Values must be
 * tracked down and freed by the user.
 */
void free_hashtable(hashtable *ht);

/*
 * Has the same effect as calling ht_destroy() on every entry in @ht, then
 * calling free_hashtable() on @ht.
 */
void destroy_hashtable(hashtable *ht);

#endif /* HASHTABLE_H */

