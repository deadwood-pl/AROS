#include <aros/kernel.h>
#include <runtime.h>

#include "kickstart.h"

/*
 * This routine runs a kickstart.
 * It's in a separate file because on some hosts (UNIX for example) the process can be
 * more complicated.
 */
int kick(int (*addr)(), struct TagItem *msg)
{
    int i = addr(msg, AROS_BOOT_MAGIC);

    DisplayError("Kernel exited with code %d", i);
    return i;
}
