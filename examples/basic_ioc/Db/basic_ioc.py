from builder import *
from iocbuilder import *


aIn('TSEC', DESC = 'Timestamp in seconds', SCAN = '1 second')


WriteRecords(sys.argv[1])
