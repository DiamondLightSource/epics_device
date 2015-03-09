# This is boilerplate for picking up the iocbuilder definitions

import sys
import os

from . import builder_path

# All this palaver is to pick up the EPICS db builder version so that it can be
# maintained properly.  A default path should be compiled into the local file
# builder_path, but this setting can be overwritten by setting the environment
# variable EPICSDBBUILDER
builder_version = os.environ.get('EPICSDBBUILDER', builder_path.path)

if builder_version == '':
    pass    # Assume db builder already on python path, do nothing more
elif builder_version[0] == '/':
    sys.path.append(builder_version)    # Explicit path to builder install
else:
    from pkg_resources import require
    require('iocbuilder==%s' % builder_version)


# Initialise the Db Builder and add our own DBD file
import epicsdbbuilder

epicsdbbuilder.InitialiseDbd(os.environ['EPICS_BASE'])
epicsdbbuilder.LoadDbdFile(
    os.path.join(os.path.dirname(__file__), '../dbd/epics_device.dbd'))
