# Support code for Python epics database generation.

import sys
import os

import epicsdbbuilder


# This class wraps the creation of records which talk directly to the EPICS
# device driver.
class EpicsDevice:
    def makeRecord(self, builder):
        builder = getattr(epicsdbbuilder.records, builder)

        def make(name, address=None, **fields):
            if address is None:
                address = name
            address = '@%s%s' % (
                self.extra_prefix,
                self.separator.join(self.address_prefix + [address]))

            record = builder(name,
                address = address,
                DTYP = 'epics_device', **fields)

            # Check for a description, make a report if none given.
            if 'DESC' not in fields:
                print >>sys.stderr, 'No description for', name

            return record

        return make

    def __init__(self):
        self.separator = ':'
        self.extra_prefix = ''
        self.address_prefix = []
        for name in [
                'longin',    'longout',
                'ai',        'ao',
                'bi',        'bo',
                'stringin',  'stringout',
                'mbbi',      'mbbo',
                'waveform']:
            setattr(self, name, self.makeRecord(name))

    # When generating a support module the address prefix can help to identify
    # the instance.
    def set_address_prefix(self, prefix):
        self.extra_prefix = prefix

    def push_name_prefix(self, prefix):
        epicsdbbuilder.PushPrefix(prefix)
        self.address_prefix.append(str(prefix))

    def pop_name_prefix(self):
        self.address_prefix.pop()
        epicsdbbuilder.PopPrefix()

    def set_name_separator(self, separator):
        epicsdbbuilder.SetSeparator(separator)
        self.separator = separator


EpicsDevice = EpicsDevice()

set_address_prefix = EpicsDevice.set_address_prefix
push_name_prefix   = EpicsDevice.push_name_prefix
pop_name_prefix    = EpicsDevice.pop_name_prefix
set_name_separator = EpicsDevice.set_name_separator


# ----------------------------------------------------------------------------
#           Record Generation Support
# ----------------------------------------------------------------------------

# Functions for creating records

# For some applications it's more convenient for MDEL to be configured so that
# by default ai and longin records always generate updates; this can be
# configured by setting this variable to -1
MDEL_default = None
def set_MDEL_default(default):
    global MDEL_default
    MDEL_default = default

# In some applications it's useful to have the ability to modify the name of
# output records.  This hook implements this behaviour.
def OutName(name):
    return name
def set_out_name(function):
    global OutName
    OutName = function


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
    fields.setdefault('MDEL', MDEL_default)
    return EpicsDevice.ai(name,
        LOPR = LOPR, HOPR = HOPR, EGU = EGU, PREC = PREC, **fields)

def aOut(name, DRVL=None, DRVH=None, EGU=None, PREC=None, **fields):
    set_out_defaults(fields, name)
    set_scalar_out_defaults(fields, DRVL, DRVH)
    return EpicsDevice.ao(OutName(name), EGU = EGU, PREC = PREC, **fields)

def longIn(name, LOPR=None, HOPR=None, EGU=None, **fields):
    fields.setdefault('MDEL', MDEL_default)
    return EpicsDevice.longin(name,
        LOPR = LOPR, HOPR = HOPR, EGU = EGU, **fields)

def longOut(name, DRVL=None, DRVH=None, EGU=None, **fields):
    set_out_defaults(fields, name)
    set_scalar_out_defaults(fields, DRVL, DRVH)
    return EpicsDevice.longout(OutName(name), EGU = EGU, **fields)


def boolIn(name, ZNAM=None, ONAM=None, **fields):
    return EpicsDevice.bi(name, ZNAM = ZNAM, ONAM = ONAM, **fields)

def boolOut(name, ZNAM=None, ONAM=None, **fields):
    set_out_defaults(fields, name)
    return EpicsDevice.bo(OutName(name), ZNAM = ZNAM, ONAM = ONAM, **fields)


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
    return EpicsDevice.mbbo(OutName(name), **fields)


def stringIn(name, **fields):
    return EpicsDevice.stringin(name, **fields)

def stringOut(name, **fields):
    set_out_defaults(fields, name)
    return EpicsDevice.stringout(OutName(name), **fields)


def Waveform(name, length, FTVL='LONG', **fields):
    return EpicsDevice.waveform(name, NELM = length, FTVL = FTVL, **fields)

def WaveformOut(name, *args, **fields):
    fields.setdefault('PINI', 'YES')
    fields.setdefault('address', name)
    return Waveform(OutName(name), *args, **fields)


__all__ = [
    'aIn',      'aOut',     'boolIn',   'boolOut',  'longIn',   'longOut',
    'mbbIn',    'mbbOut',   'stringIn', 'stringOut',
    'Waveform', 'WaveformOut',
    'EpicsDevice', 'set_MDEL_default', 'set_out_name',
    'set_address_prefix', 'push_name_prefix', 'pop_name_prefix']
