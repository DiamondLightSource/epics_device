import sys, os
sys.path.append(os.environ['EPICS_DEVICE'])

from epics_device import *
SetTemplateRecordNames()

longIn('TSEC', DESC = 'Timestamp in seconds', SCAN = '1 second')

# Write out the generated .db file
WriteRecords(sys.argv[1])
