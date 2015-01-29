from builder import *


WF_LENGTH = 128

# In this simple example setting FREQ_S causes WF and SUM to update.
aOut('FREQ',
    PINI = 'YES',
    FLNK = create_fanout('WFFAN',
        Waveform('WF', WF_LENGTH, 'DOUBLE'),
        aIn('SUM')))

Action('WRITE')
Action('FAIL')


WriteRecords(sys.argv[1])
