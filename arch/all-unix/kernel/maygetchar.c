/*
    Copyright (C) 1995-2021, The AROS Development Team. All rights reserved.
*/

#include <aros/libcall.h>

#include "kernel_base.h"
#include "kernel_intern.h"

#include <unistd.h>

AROS_LH0(int, KrnMayGetChar,
         struct KernelBase *, KernelBase, 26, Kernel)
{
    AROS_LIBFUNC_INIT
    
    char c;
    int ret;

    ret = KernelBase->kb_PlatformData->iface->read (STDERR_FILENO, &c, 1);
    AROS_HOST_BARRIER

    return (ret == 1) ? c : -1;

    AROS_LIBFUNC_EXIT
}
