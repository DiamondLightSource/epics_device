/* Record type definitions for the records used by epics_device. */

/* It seems mad that I have to declare the structures here.  Why on earth
 * aren't they declared somewhere in an EPICS header?!  Unfortunately it seems
 * that the only place these structures are defined is in the corresponding
 * base/rec/<type>Record.c implementation file.  Grrr. */

#define COMMON_FIELDS(type) \
    long number; \
    long (*dev_report)(int); \
    long (*init)(int); \
    long (*init_record)(type##Record *); \
    long (*get_ioint_info)(int, dbCommon *, IOSCANPVT *)

struct longinDevice {
    COMMON_FIELDS(longin);
    long (*read_longin)(longinRecord *);
};

struct longoutDevice {
    COMMON_FIELDS(longout);
    long (*write_longout)(longoutRecord *);
};

struct aiDevice {
    COMMON_FIELDS(ai);
    long (*read_ai)(aiRecord *);
    long (*special_linconv)(aiRecord *, int);
};

struct aoDevice {
    COMMON_FIELDS(ao);
    long (*write_ao)(aoRecord *);
    long (*special_linconv)(aoRecord *, int);
};

struct biDevice {
    COMMON_FIELDS(bi);
    long (*read_bi)(biRecord *);
};

struct boDevice {
    COMMON_FIELDS(bo);
    long (*write_bo)(boRecord *);
};

struct stringinDevice {
    COMMON_FIELDS(stringin);
    long (*read_stringin)(stringinRecord *);
};

struct stringoutDevice {
    COMMON_FIELDS(stringout);
    long (*write_stringout)(stringoutRecord *);
};

struct mbbiDevice {
    COMMON_FIELDS(mbbi);
    long (*read_mbbi)(mbbiRecord *);
};

struct mbboDevice {
    COMMON_FIELDS(mbbo);
    long (*write_mbbo)(mbboRecord *);
};

struct waveformDevice {
    COMMON_FIELDS(waveform);
    long (*read_waveform)(waveformRecord *);
};

#undef COMMON_FIELDS
