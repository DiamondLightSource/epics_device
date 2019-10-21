TOP = ..

export TOP

include $(TOP)/configure/RELEASE
export EPICS_BASE

install:
	sphinx-build -nE -b html . html

clean:
	rm -rf html

.PHONY: clean install
