# Example startup script for vxWorks IOC

# Minimal line editing support
tyBackspaceSet(127)

# Get NTP up and running.  Without this the timers in the persistence thread
# will hang!
putenv "EPICS_TS_MIN_WEST=0"
putenv "EPICS_TS_NTP_INET=172.23.240.2"

# Configure persistent state location
hostAdd "serv0005.cs.diamond.ac.uk", "172.23.240.5"
nfsAuthUnixSet "serv0005.cs.diamond.ac.uk", 37134, 500, 0, 0
nfsMount "serv0005.cs.diamond.ac.uk", "/exports/iocs", "/iocs"

# Finally bring up the IOC
ld < example_ioc.munch
rebootHookAdd(epicsExitCallAtExits)
cd "../.."
vxWorksMain("/iocs/autosave/TS-DI-IOC-02/persist", 10)

# vim: set filetype=sh:
