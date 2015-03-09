TOP = ..
include $(TOP)/configure/CONFIG

export EPICSDBBUILDER

install: builder_path.py

builder_path.py: builder.py $(TOP)/configure/CONFIG_SITE
	$(PYTHON) $< >$@

clean:
	rm -f builder_path.py *.pyc

.PHONY: install clean
.DELETE_ON_ERROR: builder_path.py
