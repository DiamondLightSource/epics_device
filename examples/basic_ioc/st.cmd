
dbLoadDatabase("dbd/basic_ioc.dbd", NULL, NULL)
basic_ioc_registerRecordDeviceDriver(pdbbase)
dbLoadRecords("db/basic_ioc.db", "DEVICE=TS-TS-TEST-99")
iocInit()
