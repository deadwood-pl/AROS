/*
    Copyright (C) 1995-2001, The AROS Development Team. All rights reserved.

    Desc: Write a big endian long (32bit) from a streamhook
*/

#include <proto/dos.h>

/******************************************************************************

    NAME */
#include <stdio.h>
#include <aros/bigendianio.h>
#include <proto/alib.h>

        BOOL WriteLong (

/*  SYNOPSIS */
        struct Hook * hook,
        ULONG         data,
        void        * stream)

/*  FUNCTION
        Writes one big endian 32bit value to a streamhook.

    INPUTS
        hook - Write to this streamhook
        data - Data to be written
        stream - Stream passed to streamhook

    RESULT
        The function returns TRUE on success and FALSE otherwise.
        See IoErr() for the reason in case of an error.

    NOTES
        This function writes big endian values to a file even on little
        endian machines.

    EXAMPLE

    BUGS

    SEE ALSO
        ReadByte(), ReadWord(), ReadLong(), ReadFloat(), ReadDouble(),
        ReadString(), ReadStruct(), WriteByte(), WriteWord(), WriteLong(),
        WriteFloat(), WriteDouble(), WriteString(), WriteStruct()

    HISTORY

******************************************************************************/
{
    UBYTE * ptr;
    struct BEIOM_Write wr={BEIO_WRITE, };

#if AROS_BIG_ENDIAN
    ptr = (UBYTE *)&data;
#   define NEXT ++
#else
    ptr = ((UBYTE *)&data) + 3;
#   define NEXT --
#endif

#define WRITE_ONE_CHAR                  \
    wr.Data = *ptr NEXT;    \
    if (CallHookA (hook, stream, &wr) == EOF) \
        return FALSE

    WRITE_ONE_CHAR;
    WRITE_ONE_CHAR;
    WRITE_ONE_CHAR;
    WRITE_ONE_CHAR;

    return TRUE;
} /* WriteLong */

