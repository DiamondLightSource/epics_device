import sys
import os

# Pick up IOCBUILDER version as defined in configure/CONFIG_SIZE
from pkg_resources import require
require('iocbuilder==%s' % os.environ['IOCBUILDER'])

# Configure the IOC builder for template generation
import iocbuilder
iocbuilder.ConfigureTemplate()

# Load EPICS device definitions into the IOC builder so we can use them
iocbuilder.ModuleVersion('epics_device',
    home = os.environ['EPICS_DEVICE'], use_name = False)

# Pull in EPICS Device record building definitions
from iocbuilder.modules.epics_device import *


longIn('TSEC', DESC = 'Timestamp in seconds', SCAN = '1 second')


# Write out the generate .db file
iocbuilder.WriteRecords(sys.argv[1])
