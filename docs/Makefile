TOP = ..

export TOP

include $(TOP)/configure/RELEASE
export EPICS_BASE

install:
	sphinx-build -b html . html

clean:
	rm -rf html

.PHONY: clean install
