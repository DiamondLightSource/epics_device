import sys, os

sys.path.append(os.environ['EPICS_DEVICE'])

from epics_device import *

set_MDEL_default(-1)
set_out_name(lambda name: name + '_S')
SetTemplateRecordNames()


WF_LENGTH = 128

# In this simple example setting FREQ_S causes WF and SUM to update.
aOut('FREQ', PREC = 4,
    PINI = 'YES',
    FLNK = create_fanout('WFFAN',
        Waveform('WF', WF_LENGTH, 'DOUBLE', DESC = 'Sine wave'),
        aIn('SUM', PREC = 3, DESC = 'Sum of sine wave')),
    DESC = 'Waveform frequency')

Action('WRITE', DESC = 'Force update to persistent state')

aOut('INTERVAL', 1e-2, 100, 's', 2, DESC = 'Trigger interval')
aOut('SCALING', PREC = 3, DESC = 'Frequency scaling')
Trigger('TRIG',
    longIn('COUNT', DESC = 'Trigger count'),
    Waveform('TRIGWF', WF_LENGTH, DESC = 'Triggered waveform'))
Action('RESET', DESC = 'Reset trigger count')


WriteRecords(sys.argv[1])
