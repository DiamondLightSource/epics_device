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

..  function:: bool initialise_persistent_state( \
        const char *file_name, int save_interval)

    If EPICS Device persistence is to be used then this function must be called
    once before publishing any PVs with the :member:`persist` field set to
    ``true``.

    The file `file_name` need not exist when the IOC is started, but the
    directory containing it should be writeable by the IOC.  `save_interval` in
    seconds determines how frequently changed PVs are written back to the state
    file.

..  function:: bool load_persistent_state(void)

    This should be called once after publishing all PVs to EPICS device but
    before calling :func:`iocInit`.  The file named in
    :func:`initialise_persistent_state` will be loaded, if present,  to
    determine initial values for all PVs marked for persistence.

..  function:: bool update_persistent_state(void)

    This can be called to force the state file to be written if any persistent
    PVs have changed their state.

..  function:: void terminate_persistent_state(void)

    This can be called during IOC shutdown to force an orderly termination of
    the persistence state and ensure that the state file is up to date.
