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


export IOCBUILDER

# We use the python library to construct the .db file
$(COMMON_DIR)/%.db: ../%.py $(wildcard ../*.py)
	$(PYTHON) $< $@


clean::
	rm -f *.pyc
