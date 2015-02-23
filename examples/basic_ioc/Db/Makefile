TOP = ..
include $(TOP)/configure/CONFIG

DB += basic_ioc.db

include $(TOP)/configure/RULES

export IOCBUILDER

# We use the python library to construct the .db file
$(COMMON_DIR)/%.db: ../%.py $(wildcard ../*.py)
	$(PYTHON) $< $@

clean::
	rm -f *.pyc
