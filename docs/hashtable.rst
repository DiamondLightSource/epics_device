Generic Hash Table
==================

This is a reasonably high performance hash table implementation.  The API is
provided in two layers: one layer specialised for storing strings, and a more
abstract layer where the management of keys is under user specified control.

The functionality described here is defined in the header file ``hashtable.h``.

High Level API
--------------

The simple API consists of the following.

..  type:: struct hash_table

    This is an opaque type representing a hash table.  A :type:`hash_table` can
    be created by calling :func:`hash_table_create` or
    :func:`hash_table_create_generic`, and should be deleted, if required, by
    calling :func:`hash_table_destroy`.

..  function:: struct hash_table *hash_table_create(bool copy_keys)

    This creates an empty hash table for storing string to :c:type:`void *`
    mappings.  If `copy_keys` is ``false`` then keys must have lifetime at least
    equal to that of the associated value, otherwise every inserted key will be
    copied and the hash table will manage the lifetime of the copy.

..  function:: void hash_table_destroy(struct hash_table *table)

    Releases all resources associated with the hash table.  If keys are under
    hash table control they will be released.

..  function:: void *hash_table_insert( \
        struct hash_table *table, const void *key, void *value)

    Inserts mapping from `key` to `value` in `table`.  If an existing value is
    replaced it is returned, otherwise ``NULL`` is returned.

..  function:: void *hash_table_lookup( \
        struct hash_table *table, const void *key)
    bool hash_table_lookup_bool( \
        struct hash_table *table, const void *key, void **value)

    Looks up `key` in `table` and returns the value or ``NULL`` if not found.
    If it is possible for the associated value to be ``NULL`` then
    :func:`hash_table_lookup_bool` can be used to check whether the value is
    present; in this case the value is returned in ``*value``.


..  function:: void *hash_table_delete( \
        struct hash_table *table, const void *key)

    Deletes entry associated with `key` from `table`, returns the deleted entry
    if found, otherwise returns ``NULL``.

..  function:: size_t hash_table_count(struct hash_table *table)

    Returns the number of entris in `table`.

..  function:: void hash_table_resize(struct hash_table *table, size_t min_size)

    The hash table will automatically resize itself as necessary as entries are
    added, but it is not normally resized after entries are deleted, though this
    can happen as a consequence of subsequent entries.  This function will
    forcibly resize `table` to have at least `min_size` entries (plus room).

..  function:: bool hash_table_walk( \
        struct hash_table *table, int *ix, const void **key, void **value)

    This function is used to iterate through all of the entries in the table.
    Note that `table` must not be modified during the iteration, as otherwise
    entries may be skipped or repeated.  The following code will walk the given
    table calling the given function for each `key` & `value` pair in turn::

        const void *key;
        const void *value;
        int ix = 0;
        while (hash_table_walk(table, &ix, &key, &value))
            process_entry(key, value);

    Actually, it probably *is* safe to delete entries during a walk, but this is
    not guaranteed without close inspection of the code!

..  function:: void hash_table_validate(struct hash_table *table)

    This performs a sanity check on the structure of `table` and raised an
    assertion failure if the check fails.


Abstract API
------------

The fuller API including the more abstract layer adds the following.

..  function:: struct hash_table *hash_table_create_ptrs(void)

    Creates a hash table with pointers or integers (of :type:`uintptr_t`
    compatible size) as keys.  In this case keys are never copied.

..  function:: struct hash_table *hash_table_create_generic( \
        const struct hash_table_ops *ops)

    This constructs an empty hash table using given `ops` structure to define
    the handling of keys.  The `ops` structure must have lifetime at least equal
    to that of the created table.

    ..  type:: hash_t

        This is an alias for :type:`!unsigned long int`.  All hash functions use
        this type.

    ..  type:: struct hash_table_ops

        This structure contains the following fields defining the abstract
        interface to keys.

        ..  member:: hash_t (\*hash)(const void \*key)

            Computes the hash value from a key.

        ..  member:: bool (\*compare)(const void \*key1, const void \*key2)

            Compares two keys, returns ``true`` if equal.

        ..  member:: void \*(\*copy_key)(const void \*key)

            If NULL then key lifetime is under control of the user of the hash
            table, who must ensure that the key is valid for the entire time
            it is present in the table.  Otherwise this function is called when
            keys are added to the table.

        ..  member:: void (\*release_key)(void \*key)

            Also can be NULL if key lifetime is under user control, otherwise is
            called when keys are removed from the table.

    Note that value lifetime is not automatically managed through this
    interface, instead this can be done through the return values from
    :func:`hash_table_insert` and :func:`hash_table_delete`.
