/* Simple interface to IOC caput logging. */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define db_accessHFORdb_accessC     // Needed to get correct DBF_ values
#include <dbAccess.h>
#include <dbFldTypes.h>
#include <db_access.h>
#include <asTrapWrite.h>
#include <asDbLib.h>

#include "error.h"

#include "pvlogging.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                            IOC PV put logging                             */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Used to trim arrays of excessive length when logging. */
static int max_array_length;


/* Alas dbGetField is rather rubbish at formatting floating point numbers, so we
 * do that ourselves, but the rest formats ok. */
static void format_field(dbAddr *dbaddr, dbr_string_t *value)
{
#define FORMAT(type, format) \
    do { \
        type *raw = (type *) dbaddr->pfield; \
        for (int i = 0; i < length; i ++) \
            snprintf(value[i], sizeof(dbr_string_t), format, raw[i]); \
    } while (0)

    long length = dbaddr->no_elements;
    switch (dbaddr->field_type)
    {
        case DBF_FLOAT:
            FORMAT(dbr_float_t, "%.7g");
            break;
        case DBF_DOUBLE:
            FORMAT(dbr_double_t, "%.15lg");
            break;
        default:
            dbGetField(dbaddr, DBR_STRING, value, NULL, &length, NULL);
            break;
    }
#undef FORMAT
}

static void print_value(dbr_string_t *value, int length)
{
    if (length == 1)
        printf("%s", value[0]);
    else
    {
        printf("[");
        int i = 0;
        for (; i < length  &&  i < max_array_length; i ++)
        {
            if (i > 0)  printf(", ");
            printf("%s", value[i]);
        }
        if (length > max_array_length + 1)
            printf(", ...");
        if (length > max_array_length)
            printf(", %s", value[length-1]);
        printf("]");
    }
}

static void epics_pv_put_hook(asTrapWriteMessage *pmessage, int after)
{
    dbAddr *dbaddr = (dbAddr *) pmessage->serverSpecific;
    int length = (int) dbaddr->no_elements;
    dbr_string_t *value = calloc((size_t) length, sizeof(dbr_string_t));
    format_field(dbaddr, value);

    if (after)
    {
        /* Log the message after the event. */
        dbr_string_t *old_value = (dbr_string_t *) pmessage->userPvt;
        printf("%s@%s %s.%s ",
            pmessage->userid, pmessage->hostid,
            dbaddr->precord->name, dbaddr->pfldDes->name);
        print_value(old_value, length);
        printf(" -> ");
        print_value(value, length);
        printf("\n");

        free(old_value);
        free(value);
    }
    else
        /* Just save the old value for logging after. */
        pmessage->userPvt = value;
}


bool hook_pv_logging(const char *access_file, int max_length)
{
    max_array_length = max_length;
    asSetFilename("db/access.acf");
    asTrapWriteRegisterListener(epics_pv_put_hook);
    return true;
}
