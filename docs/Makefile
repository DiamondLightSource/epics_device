TOP = ..

export TOP

include $(TOP)/configure/RELEASE
export EPICS_BASE

html: $(wildcard *.rst) $(wildcard *.py)

install: html
	sphinx-build -nE -b html . html

clean:
	rm -rf html

.PHONY: clean install
