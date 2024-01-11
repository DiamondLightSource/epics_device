TOP = ..
include $(TOP)/configure/CONFIG

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# Create and install (or just install) into <top>/db
# databases, templates, substitutions like this

DB += example_ioc.db
DB += access.acf


include $(TOP)/configure/RULES

#----------------------------------------
#  ADD RULES AFTER THIS LINE

export EPICS_DEVICE

# We use the python library to construct the .db file
$(COMMON_DIR)/%.db: ../%.py $(wildcard ../*.py)
	$(PYTHON) $< $@

# EPICS 3.15 and later has changed the build system.  The following rules are
# needed to make things work.

%.db.d:
	touch $@
