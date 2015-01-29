# Common definitions for all records

import os

# It is important to import support before importing iocbuilder, as the support
# module initialises iocbuilder (and determines which symbols it exports!)
from support import *
from iocbuilder import *

__all__ = [
    'Action', 'Trigger', 'ForwardLink', 'AggregateSeverity', 'concat']


def Action(name, **kargs):
    return boolOut(name, PINI = 'NO', **kargs)

def Trigger(prefix, *pvs):
    done = Action('%s:DONE' % prefix, DESC = '%s processing done' % prefix)
    return boolIn('%s:TRIG' % prefix,
        SCAN = 'I/O Intr', DESC = '%s processing trigger' % prefix,
        FLNK = create_fanout('%s:TRIG:FAN' % prefix, *pvs + (done,)))

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
