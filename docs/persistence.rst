Support for Persistent PV State
===============================

This EPICS Device module provides a mechanism for persistent PV state which is
quite different from the mechanism provided by autosave/restore.  If
autosave/restore is used then this functionality can be ignored, and no PVs
should be marked as persistent internally.

The functionality described here is defined in the header file
``persistence.h``.

The mechanism here works by saving a single file containing the persistent state
of each PV marked for persistence.  Note that only ``epics_device`` PVs can be
marked as persistent, and the persistence marking is performed at the time the
PVs are published to the device by the IOC.

The following functions provide the persistence interface.

..  function:: bool initialise_persistent_state(int save_interval)

    If EPICS Device persistence is to be used then this function must be called
    exactly once before publishing any PVs with the :member:`persist` field set
    to ``true``.

    `save_interval` in seconds determines how frequently changed PVs are written
    back to the state file.

..  function:: bool load_persistent_state( \
        const char *file_name, bool check_parse)

    This should be called once after publishing all PVs to EPICS device but
    before calling :func:`iocInit`.  The given `file_name` will be loaded, if
    present, to determine initial values for all PVs marked for persistence, and
    after this point updates to PVs will be written at the interval determined
    by `save_interval` passed to :func:`initialise_persistent_state`.

    If `check_parse` is ``false`` then this function will return success even if
    there are parsing errors while loading the persistence file.

..  function:: bool update_persistent_state(void)

    This can be called to force the state file to be written if any persistent
    PVs have changed their state.  Returns ``false`` if an error occurs.

..  function:: void terminate_persistent_state(void)

    This can be called during IOC shutdown to force an orderly termination of
    the persistence state and ensure that the state file is up to date.
