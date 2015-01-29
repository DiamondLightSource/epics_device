/* Hash table interface. */


/* We implement the hash table as an opaque type, all the work will be done
 * inside hashtable.c. */
struct hash_table;



/* Hash table indexed by null terminated strings.  If copy_keys is false then
 * keys must have lifetime at least equal to that of the associated value,
 * otherwise every inserted key will be copied. */
struct hash_table *hash_table_create(bool copy_keys);
/* Release all resources consumed by hash table. */
void hash_table_destroy(struct hash_table *table);

/* Look up key in hash table, return NULL if not found. */
void *hash_table_lookup(struct hash_table *table, const void *key);
/* Look up key in hash table, return true if found.  Equivalent to
 * hash_table_lookup() except when value is NULL. */
bool hash_table_lookup_bool(
    struct hash_table *table, const void *key, void **value);

/* Inserts (key,value) pair in hash table.  If value is already present its old
 * value is returned before being overwritten with the new value, otherwise NULL
 * is returned. */
void *hash_table_insert(struct hash_table *table, const void *key, void *value);
/* Deletes key from hash table, returning the old value if present, otherwise
 * NULL. */
void *hash_table_delete(struct hash_table *table, const void *key);

/* Returns the number of entries in the table. */
size_t hash_table_count(struct hash_table *table);

/* Resizes hash table to have at least the given number of slots.  Can also be
 * used after deleting entries to compress table. */
void hash_table_resize(struct hash_table *table, size_t min_size);

/* Iterator for walking all entries in hash table.  The table *must* remain
 * unchanged during the walk.  Start by initialising ix to zero, each call
 * will increment ix and return the associated (key,value) pair and return
 * true until the entire table has been walked, when false will be returned.
 * Either key or value can be null if the result is not required. */
bool hash_table_walk(
    struct hash_table *table, int *ix, const void **key, void **value);


/* Sanity checking of hash table consistency, raises assert fail if any error is
 * found.  Should only fail in presence of hash table bug, memory overwrite, or
 * key lifetime mismanagement. */
void hash_table_validate(struct hash_table *table);



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Optional generic hash table interface. */

/* Hashes are 64-bits. */
typedef uint64_t hash_t;


/* Abstract key management interface so hash table can support key types other
 * than null terminated strings. */
struct hash_table_ops {
    /* Returns hash value for given key. */
    hash_t (*hash)(const void *key);
    /* Returns true iff both keys compare equal. */
    bool (*compare)(const void *key1, const void *key2);

    /* Key lifetime is either under control of the user of the hash table or the
     * hash table.  Caller control is indicated by leaving copy_key() and
     * release_key() NULL, otherwise these will be called when keys are added
     * and removed respectively. */
    /* Creates a copy of key for storage in hash table. */
    void *(*copy_key)(const void *key);
    /* Releases a previously copied key. */
    void (*release_key)(void *key);
};


/* Create fresh hash table.  Most general form with generic key ops table. */
struct hash_table *hash_table_create_generic(const struct hash_table_ops *ops);

/* Hash table keyed by pointers.  Also use this for integer indexed tables. */
struct hash_table *hash_table_create_ptrs(void);
