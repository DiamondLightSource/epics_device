TOP = ..

export TOP

include $(TOP)/configure/RELEASE
include $(TOP)/configure/CONFIG_SITE
export EPICS_BASE

html: $(wildcard *.rst) $(wildcard *.py)

install: html
	$(SPHINXBUILD) -nE -b html . html

clean:
	rm -rf html

.PHONY: clean install
