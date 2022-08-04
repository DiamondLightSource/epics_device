# Common definitions for all records

from epicsdbbuilder import *
from .device import *

__all__ = [
    'Action', 'Trigger', 'ForwardLink', 'AggregateSeverity', 'concat']


def Action(name, **kargs):
    return boolOut(name, PINI = 'NO', **kargs)

def Trigger(prefix, *pvs, **kargs):
    done = Action('%s:DONE' % prefix, DESC = '%s processing done' % prefix)
    trigger = boolIn('%s:TRIG' % prefix,
        SCAN = 'I/O Intr', DESC = '%s processing trigger' % prefix,
        FLNK = create_fanout('%s:TRIG:FAN' % prefix, *pvs + (done,)))

    # Configure all PVs to pick up their timestamp from trigger so that at least
    # they all show a consistent timestamp.  We leave :DONE because the
    # difference it shows may be instructive.
    for pv in pvs:
        pv.TSEL = trigger.TIME

    # If set_time is set the IOC driver will be computing the trigger timestamp
    if kargs.pop('set_time', False):
        trigger.TSE = -2

    assert kargs == {}, 'Unexpected keyword args: %s' % list(kargs.keys())
    return trigger


# Creates an Action with the given name which is triggered every time any of the
# listed PVs is processed.
def ForwardLink(name, desc, *pvs, **kargs):
    action = Action(name, DESC = desc, **kargs)
    for pv in pvs:
        pv.FLNK = action
    return action


# Aggregates the severity of all the given records into a single record.  The
# value of the record is constant, but its SEVR value reflects the maximum
# severity of all of the given records.
def AggregateSeverity(name, description, recs):
    assert len(recs) <= 12, 'Too many records to aggregate'
    return records.calc(name,
        CALC = 1, DESC = description,
        # Assign each record of interest to a calc record input with MS.
        # This then automatically propagates to the severity of the whole
        # record.
        **dict([
            ('INP' + c, MS(r))
            for c, r in zip ('ABCDEFGHIJKL', recs)]))

# Concatenates a list of lists
def concat(ll):
    return [x for l in ll for x in l]
