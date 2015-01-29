# This is boilerplate for picking up the iocbuilder definitions

import sys
import os


# All this palaver is to pick up the IOC builder version from
# configure/RELEASE so that it can be maintained properly.
builder_version = os.environ['IOCBUILDER']
if builder_version == '':
    pass    # Assume iocbuilder already on python path, do nothing more
elif builder_version[0] == '/':
    sys.path.append(builder_version)    # Explicit path to builder install
else:
    from pkg_resources import require
    require('iocbuilder==%s' % builder_version)


# Configure the IOC builder as we want to use it and load epics_device
from iocbuilder import ModuleVersion, TemplateRecordNames, ConfigureTemplate
ConfigureTemplate(record_names = TemplateRecordNames())
ModuleVersion('epics_device',
    home = os.environ['EPICS_DEVICE'], use_name = False)

from iocbuilder import *
from iocbuilder.modules.epics_device.common import *
