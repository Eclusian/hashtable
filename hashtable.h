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

typedef struct hashtable_t hashtable;

#define HT_VARIABLE_SIZE (-1)

// Return a new empty hashtable where keys have size keysize.
// Keys with size larger than keysize will be truncated to fit into keysize.
// Keys smaller than keysize should not be used unless padding is enabled.
// If pad is true, keys smaller than keysize are allowed but
// must be null-terminated.
// If pad is false, then using keys smaller than keysize will result in
// undefined behavior.
// If keysize is HT_VARIABLE_SIZE, pad is ignored, and keys are hashed until
// encountering a null byte (useful for string keys).
hashtable * new_hashtable(ssize_t keysize, int pad);

// Put a new key, value pair into the hashtable.
// An instance of key is allocated within the table, but only a pointer to
// value is stored, so the lifetime of value should extend until it is removed
// from the table with ht_remove(), or until the hashtable itself is freed.
// If key already exists, the corresponding value is overwritten.
// Returns 0 on success, 1 on failure.
int ht_put(const void * key, void * value, hashtable ** restrict hthandle);

// Retrieve a pointer to the value mapped to by key.
// If key is invalid, return NULL.
void * ht_get(const void * key, hashtable * restrict ht);

// Remove an entry from the hashtable, freeing memory occupied by the key.
// If key is invalid or no key is found, return NULL
void * ht_remove(const void * key, hashtable * restrict ht);

// Return a malloc'd array of all values in the table, and set len to the length of the array.
// Set len to 0 and return NULL if ht is empty.
void * ht_values(hashtable * ht, size_t * len);

// Returns a truthy value if ht is empty, falsy value otherwise.
int ht_isempty(hashtable * ht);

// Returns the number of elements currently stored in the hashtable.
size_t ht_nitems(hashtable * ht);

// Free all memory utilized by the hashtable and stored keys.
// values of hashtable entries are NOT freed by this function; values must be
// tracked and freed by the user.
void free_hashtable(hashtable * ht);


#endif /* HASHTABLE_H */

