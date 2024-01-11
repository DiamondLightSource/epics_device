TOP = ..
include $(TOP)/configure/CONFIG

DB += basic_ioc.db

include $(TOP)/configure/RULES

export EPICS_DEVICE

# We use the python library to construct the .db file
$(COMMON_DIR)/%.db: ../%.py $(wildcard ../*.py)
	$(PYTHON) $< $@

# EPICS 3.15 and later has changed the build system.  The following rules are
# needed to make things work.

%.db.d:
	touch $@
