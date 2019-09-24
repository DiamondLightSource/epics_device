/* Hash table implementation. */

#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

#include "hashtable.h"


#define EMPTY_HASH      0               // Marks unused slot
#define DELETED_HASH    ((hash_t) -1)   // Marks deleted slot, "tombstone"


struct table_entry {
    hash_t hash;
    const void *key;    // Null if absent
    void *value;
};

struct hash_table {
    const struct hash_table_ops *key_ops;   // Key operations
    size_t entries;     // Number of entries in table
    size_t deleted;     // Number of deleted entries in table
    size_t size_mask;   // True size is power of 2, mask selects modulo size
    struct table_entry *table;
};


#define INITIAL_SIZE    8


/* Hash algorithm lifted from Python Objects/stringobject.c:string_hash. */
static hash_t hash_string(const void *key)
{
    const char *s = key;
    size_t length = strlen(s);
    if (length == 0)
        return 0;
    else
    {
        hash_t hash = (hash_t) *s++ << 7;
        for (size_t i = 1; i < length; i++)
            hash = (1000003 * hash) ^ (hash_t) (unsigned int) *s++;
        return hash ^ length;
    }
}


static bool compare_string(const void *key1, const void *key2)
{
    return strcmp(key1, key2) == 0;
}


static hash_t hash_ptr(const void *key)
{
    return (hash_t) key;
}

static bool compare_ptr(const void *key1, const void *key2)
{
    return key1 == key2;
}


/* Key lifetime managed by hash table. */
static struct hash_table_ops copy_string_ops = {
    .hash = hash_string,
    .compare = compare_string,
    .copy_key = (void *(*)(const void *)) strdup,   // void *(*)(const char *)
    .release_key = free,
};

/* Key lifetime managed by caller. */
static struct hash_table_ops keep_string_ops = {
    .hash = hash_string,
    .compare = compare_string,
};

/* Pointer keys, no lifetime management required. */
static struct hash_table_ops ptr_ops = {
    .hash = hash_ptr,
    .compare = compare_ptr,
};



struct hash_table *hash_table_create_generic(const struct hash_table_ops *ops)
{
    struct hash_table *table = malloc(sizeof(struct hash_table));
    *table = (struct hash_table) {
        .key_ops = ops,
        .entries = 0,
        .deleted = 0,
        .size_mask = INITIAL_SIZE - 1,
        .table = calloc(INITIAL_SIZE, sizeof(struct table_entry)),
    };
    return table;
}


struct hash_table *hash_table_create_ptrs(void)
{
    return hash_table_create_generic(&ptr_ops);
}


struct hash_table *hash_table_create(bool copy_keys)
{
    return hash_table_create_generic(
        copy_keys ? &copy_string_ops : &keep_string_ops);
}


/* Two hash values are reserved for special meanings in hash table entries, this
 * wrapper ensures they're never generated. */
static hash_t compute_hash(struct hash_table *table, const void *key)
{
    hash_t hash = table->key_ops->hash(key);
    if (hash == EMPTY_HASH  ||  hash == DELETED_HASH)
        hash = (hash_t) -2;
    return hash;
}


/* Helper for checking whether a table entry is occupied. */
static bool empty_entry(struct table_entry *entry)
{
    return entry->hash == EMPTY_HASH  ||  entry->hash == DELETED_HASH;
}


/* Helper for calling the release_key method.  As we want the underlying key
 * interface to work with const pointers somewhere we need to remove the const
 * pointer on copied keys.  Here is where it's done. */
static void release_key(struct hash_table *table, const void *key)
{
    table->key_ops->release_key(CAST_FROM_TO(const void *, void *, key));
}


void hash_table_destroy(struct hash_table *table)
{
    if (table->key_ops->release_key)
    {
        for (size_t i = 0; i <= table->size_mask; i++)
        {
            struct table_entry *entry = &table->table[i];
            if (!empty_entry(entry))
                release_key(table, entry->key);
        }
    }
    free(table->table);
    free(table);
}


/* Core lookup process: returns entry containing key or where it can be put,
 * also sets flag indicating if value was found. */
static struct table_entry *lookup(
    struct hash_table *table, const void *key, hash_t hash, bool *found)
{
    /* Walk the hash table taking all bits of the hash value into account.
     * Algorithm taken from Python Objects/dictobject.c:lookupdict. */
    hash_t perturb = hash;
    struct table_entry *deleted_entry = NULL;
    for (size_t ix = (size_t) hash; ;
         perturb >>= 5, ix = 1 + 5*ix + (size_t) perturb)
    {
        struct table_entry *entry = &table->table[ix & table->size_mask];
        if (entry->hash == hash  &&  table->key_ops->compare(key, entry->key))
        {
            /* Match. */
            *found = true;
            return entry;
        }
        else if (entry->hash == EMPTY_HASH)
        {
            /* End of hash chain.  Return this entry, or the first deleted entry
             * if one was found. */
            *found = false;
            if (deleted_entry)
                return deleted_entry;
            else
                return entry;
        }
        else if (entry->hash == DELETED_HASH  &&  deleted_entry == NULL)
            deleted_entry = entry;
    }
}


void *hash_table_lookup(struct hash_table *table, const void *key)
{
    bool found;
    return lookup(table, key, compute_hash(table, key), &found)->value;
}


bool hash_table_lookup_bool(
    struct hash_table *table, const void *key, void **value)
{
    bool found;
    *value = lookup(table, key, compute_hash(table, key), &found)->value;
    return found;
}


/* Called when the table has become too full to expand the table by doubling its
 * size.  Can also be called at any other time to force a rebuild of a table
 * that needs repacking because of deleted entries. */
static void resize_table(struct hash_table *table, size_t min_size)
{
    /* Compute new size taking deleted items into account: next power of two
     * with at least 50% table free. */
    size_t entries = table->entries - table->deleted;
    if (min_size < 2 * entries)
        min_size = 2 * entries;
    size_t new_size = INITIAL_SIZE;
    while (new_size < min_size)
        new_size <<= 1;

    /* Local instance of new hash table so we can use lookup to insert. */
    struct hash_table new_table = {
        .key_ops = table->key_ops,
        .size_mask = new_size - 1,
        .entries = entries,
        .deleted = 0,
        .table = calloc(new_size, sizeof(struct table_entry)) };
    for (size_t ix = 0; ix <= table->size_mask; ix ++)
    {
        struct table_entry *entry = &table->table[ix];
        if (!empty_entry(entry))
        {
            bool found;
            struct table_entry *new_entry =
                lookup(&new_table, entry->key, entry->hash, &found);
            new_entry->hash = entry->hash;
            new_entry->key = entry->key;
            new_entry->value = entry->value;
        }
    }

    /* Update hash table. */
    free(table->table);
    *table = new_table;
}


void *hash_table_insert(struct hash_table *table, const void *key, void *value)
{
    hash_t hash = compute_hash(table, key);
    bool found;
    struct table_entry *entry = lookup(table, key, hash, &found);

    /* Proper management of deleted and entry counts. */
    if (entry->hash == EMPTY_HASH)
        /* Adding a new entry. */
        table->entries += 1;
    else if (entry->hash == DELETED_HASH)
        /* Overwriting a deleted key. */
        table->deleted -= 1;
    /* Otherwise updated a value in place, no action required. */

    if (!found)
    {
        /* New entry, set up key and hash, copying key if necessary. */
        entry->hash = hash;
        if (table->key_ops->copy_key)
            entry->key = table->key_ops->copy_key(key);
        else
            entry->key = key;
    }
    else if (!table->key_ops->copy_key)
        /* Key lifetime determined by caller.  New key replaces old. */
        entry->key = key;

    void *old_value = entry->value;
    entry->value = value;

    /* Check for over-full hash table, expand if necessary, if more than 2/3
     * full. */
    if (3 * table->entries >= 2 * table->size_mask)
        resize_table(table, 0);

    return old_value;
}


void *hash_table_delete(struct hash_table *table, const void *key)
{
    bool found;
    struct table_entry *entry =
        lookup(table, key, compute_hash(table, key), &found);
    void *old_value = entry->value;
    if (found)
    {
        if (table->key_ops->release_key)
            release_key(table, entry->key);
        entry->hash = DELETED_HASH;
        entry->key = NULL;
        entry->value = NULL;
        table->deleted += 1;
    }
    return old_value;
}


size_t hash_table_count(struct hash_table *table)
{
    return table->entries - table->deleted;
}


void hash_table_resize(struct hash_table *table, size_t min_size)
{
    resize_table(table, min_size);
}


bool hash_table_walk(
    struct hash_table *table, int *ix, const void **key, void **value)
{
// printf("hash_table_walk %p, %p(%d), %p, %p\n", table, ix, *ix, key, value);
    if (*ix < 0)
        return false;

    for (; *ix <= (int) table->size_mask; (*ix) ++)
    {
        struct table_entry *entry = &table->table[*ix];
        if (!empty_entry(entry))
        {
            if (key)
                *key = entry->key;
            if (value)
                *value = entry->value;
            (*ix) += 1;
            return true;
        }
    }
    return false;
}


#if 0
void hash_table_validate(struct hash_table *table)
{
    size_t entries = 0;
    size_t deleted = 0;
    size_t size = table->size_mask + 1;
    ASSERT_OK((size & -size) == size);      // Size a power of 2
    for (size_t i = 0; i < size; i ++)
    {
        struct table_entry *entry = &table->table[i];
        if (entry->hash != EMPTY_HASH)
        {
            entries += 1;
            if (entry->hash == DELETED_HASH)
                deleted += 1;
            else
            {
                ASSERT_OK(entry->hash == compute_hash(table, entry->key));
                bool found;
                ASSERT_OK(lookup(
                    table, entry->key, entry->hash, &found) == entry);
                ASSERT_OK(found);
            }
        }
    }
    ASSERT_OK(entries == table->entries);
    ASSERT_OK(deleted == table->deleted);
    ASSERT_OK(3 * table->entries < 2 * table->size_mask);
}
#endif
