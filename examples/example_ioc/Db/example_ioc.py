import sys, os

sys.path.append(os.environ['EPICS_DEVICE'])

from epics_device import *

set_MDEL_default(-1)
set_out_name(lambda name: name + '_S')
SetTemplateRecordNames()

WF_LENGTH = 128

# In this simple example setting FREQ_S causes WF and SUM to update.
aOut('FREQ', PREC = 4,
    FLNK = create_fanout('WFFAN',
        Waveform('WF', WF_LENGTH, 'DOUBLE', DESC = 'Sine wave'),
        aIn('SUM', PREC = 3, DESC = 'Sum of sine wave')),
    DESC = 'Waveform frequency')

Trigger('TRIG',
    Waveform('TRIGWF', WF_LENGTH, DESC = 'Triggered waveform'),
    longIn('COUNT', DESC = 'Trigger count'))
Action('RESET', DESC = 'Reset trigger count')
aOut('INTERVAL', 1e-2, 100, 's', 2, DESC = 'Trigger interval')
aOut('SCALING', PREC = 3, DESC = 'Frequency scaling')

Action('WRITE', DESC = 'Force update to persistent state')

for prefix in ['A', 'B']:
    push_name_prefix(prefix)
    read = longIn('READ')
    write = longOut('WRITE', FLNK = read)
    pop_name_prefix()

longOut('ADD_ONE', DESC = 'Adds one to the written value')

WriteRecords(sys.argv[1])
