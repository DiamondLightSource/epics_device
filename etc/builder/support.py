# Support code for Python epics database generation.

import sys
import os


# This class wraps the creation of records which talk directly to the
# Libera device driver.
class EpicsDevice(Device):
    DbdFileList = ['epics_device']
    LibFileList = ['epics_device']

    class makeRecord:
        def __init__(self, builder, addr_name):
            self.builder = getattr(records, builder)
            self.addr_name = addr_name

        def __call__(self, name, address=None, **fields):
            if address is None:
                address = name
            record = self.builder(name, **fields)
            record.DTYP = 'epics_device'
            setattr(record, self.addr_name, '@' + address)

            # Check for a description, make a report if none given.
            if 'DESC' not in fields:
                print 'No description for', name

            return record

    @classmethod
    def init(cls):
        for name, addr in [
                ('longin',    'INP'), ('longout',   'OUT'),
                ('ai',        'INP'), ('ao',        'OUT'),
                ('bi',        'INP'), ('bo',        'OUT'),
                ('stringin',  'INP'), ('stringout', 'OUT'),
                ('mbbi',      'INP'), ('mbbo',      'OUT'),
                ('waveform',  'INP')]:
            setattr(cls, name, cls.makeRecord(name, addr))

EpicsDevice.init()
EpicsDevice()



# ----------------------------------------------------------------------------
#           Record Generation Support
# ----------------------------------------------------------------------------

# Functions for creating records

# Helper for output records: turns out we want quite a uniform set of defaults.
def set_out_defaults(fields, name):
    fields.setdefault('address', name)
    fields.setdefault('OMSL', 'supervisory')
    fields.setdefault('PINI', 'YES')

# For longout and ao we want DRV{L,H} to match {L,H}OPR by default.  Also puts
# all settings into fields for convenience.
def set_scalar_out_defaults(fields, DRVL, DRVH):
    fields['DRVL'] = DRVL
    fields['DRVH'] = DRVH
    fields.setdefault('LOPR', DRVL)
    fields.setdefault('HOPR', DRVH)


def aIn(name, LOPR=None, HOPR=None, EGU=None, PREC=None, **fields):
    fields.setdefault('MDEL', -1)
    return EpicsDevice.ai(name,
        LOPR = LOPR, HOPR = HOPR, EGU  = EGU,  PREC = PREC, **fields)

def aOut(name, DRVL=None, DRVH=None, EGU=None, PREC=None, **fields):
    set_out_defaults(fields, name)
    set_scalar_out_defaults(fields, DRVL, DRVH)
    return EpicsDevice.ao(name + '_S',
        EGU  = EGU,  PREC = PREC, **fields)

def longIn(name, LOPR=None, HOPR=None, EGU=None, **fields):
    fields.setdefault('MDEL', -1)
    return EpicsDevice.longin(name,
        LOPR = LOPR, HOPR = HOPR, EGU = EGU, **fields)

def longOut(name, DRVL=None, DRVH=None, EGU=None, **fields):
    set_out_defaults(fields, name)
    set_scalar_out_defaults(fields, DRVL, DRVH)
    return EpicsDevice.longout(name + '_S', EGU = EGU, **fields)


def boolIn(name, ZNAM=None, ONAM=None, **fields):
    return EpicsDevice.bi(name, ZNAM = ZNAM, ONAM = ONAM, **fields)

def boolOut(name, ZNAM=None, ONAM=None, **fields):
    set_out_defaults(fields, name)
    return EpicsDevice.bo(name + '_S', ZNAM = ZNAM, ONAM = ONAM, **fields)


# Field name prefixes for mbbi/mbbo records.
mbb_prefixes = [
    'ZR', 'ON', 'TW', 'TH', 'FR', 'FV', 'SX', 'SV',     # 0-7
    'EI', 'NI', 'TE', 'EL', 'TV', 'TT', 'FT', 'FF']     # 8-15

# Adds a list of (option, value [,severity]) tuples into field settings
# suitable for mbbi and mbbo records.
def process_mbb_values(fields, option_values):
    def process_value(prefix, option, value, severity=None):
        fields[prefix + 'ST'] = option
        fields[prefix + 'VL'] = value
        if severity:
            fields[prefix + 'SV'] = severity

    for prefix, (default, option_value) in \
            zip(mbb_prefixes, enumerate(option_values)):
        if isinstance(option_value, tuple):
            process_value(prefix, *option_value)
        else:
            process_value(prefix, option_value, default)

def mbbIn(name, *option_values, **fields):
    process_mbb_values(fields, option_values)
    return EpicsDevice.mbbi(name, **fields)

def mbbOut(name, *option_values, **fields):
    process_mbb_values(fields, option_values)
    set_out_defaults(fields, name)
    return EpicsDevice.mbbo(name + '_S', **fields)


def stringIn(name, **fields):
    return EpicsDevice.stringin(name, **fields)

def stringOut(name, **fields):
    set_out_defaults(fields, name)
    return EpicsDevice.stringout(name + '_S', **fields)


def Waveform(name, length, FTVL='LONG', **fields):
    return EpicsDevice.waveform(name, NELM = length, FTVL = FTVL, **fields)

def WaveformOut(address, *args, **fields):
    fields.setdefault('PINI', 'YES')
    # A hacky trick: if the given name ends with _S then use that for both the
    # PV name and its address, otherwise add _S to the PV name.  This allows an
    # easy choice of two options for the address.
    if address[-2:] == '_S':
        name = address
    else:
        name = address + '_S'
    return Waveform(name, address = address, *args, **fields)


__all__ = [
    'aIn',      'aOut',     'boolIn',   'boolOut',  'longIn',   'longOut',
    'mbbIn',    'mbbOut',   'stringIn', 'stringOut',
    'Waveform', 'WaveformOut']
