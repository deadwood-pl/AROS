/*
    (C) 1995-96 AROS - The Amiga Replacement OS
    $Id$
    $Log$
    Revision 1.6  1997/10/08 00:44:07  srittau
    Set secondary error

    Revision 1.5  1997/01/27 00:36:17  ldp
    Polish

    Revision 1.4  1996/10/24 15:32:56  aros
    Added missing include

    Revision 1.3  1996/09/21 14:14:23  digulla
    Hand DOSBase to DoName()

    Revision 1.2  1996/09/13 17:50:06  digulla
    Use IPTR

    Revision 1.1  1996/09/11 12:54:45  digulla
    A couple of new DOS functions from M. Fleischer

    Desc:
    Lang: english
*/
#include <exec/memory.h>
#include <proto/exec.h>
#include <dos/dosextens.h>
#include <dos/filesystem.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <string.h>
#include "dos_intern.h"

LONG DoName(struct IOFileSys *iofs, STRPTR name, struct DosLibrary * DOSBase)
{
    /* extern struct DosLibrary *DOSBase; */
    STRPTR volname, pathname, s1;
    BPTR cur;
    struct DosList *dl;
    struct Device *device;
    struct Unit *unit;
    struct FileHandle *fh;
    struct Process *me=(struct Process *)FindTask(NULL);

    if(!Strnicmp(name,"PROGDIR:",8))
    {
	cur=me->pr_HomeDir;
	volname=NULL;
	pathname=name+8;
    }else
    {
	/* Copy volume name */
	cur=me->pr_CurrentDir;
	s1=name;
	pathname=name;
	volname=NULL;
	while(*s1)
	    if(*s1++==':')
	    {
		volname=(STRPTR)AllocMem(s1-name,MEMF_ANY);
		if(volname==NULL)
		{
		    SetIoErr(ERROR_NO_FREE_STORE);
		    return ERROR_NO_FREE_STORE;
		}
		CopyMem(name,volname,s1-name-1);
		volname[s1-name-1]=0;
		pathname=s1;
		break;
	    }
    }

    dl=LockDosList(LDF_ALL|LDF_READ);
    if(volname!=NULL)
    {
	/* Find logical device */
	dl=FindDosEntry(dl,volname,LDF_DEVICES|LDF_VOLUMES|LDF_ASSIGNS);
	if(dl==NULL)
	{
	    UnLockDosList(LDF_ALL|LDF_READ);
	    FreeMem(volname,s1-name);
	    SetIoErr(ERROR_DEVICE_NOT_MOUNTED);
	    return ERROR_DEVICE_NOT_MOUNTED;
	}
	device=dl->dol_Device;
	unit  =dl->dol_Unit;
    }else if(cur)
    {
	fh=(struct FileHandle *)BADDR(cur);
	device=fh->fh_Device;
	unit  =fh->fh_Unit;
    }else
    {
	device=DOSBase->dl_NulHandler;
	unit  =DOSBase->dl_NulLock;
    }

    iofs->IOFS.io_Device =device;
    iofs->IOFS.io_Unit	 =unit;
    iofs->io_Args[0]=(IPTR)pathname;

    /* Send the request. */
    DoIO(&iofs->IOFS);

    if(dl!=NULL)
	UnLockDosList(LDF_ALL|LDF_READ);

    if(volname!=NULL)
	FreeMem(volname,s1-name);

    SetIoErr(iofs->io_DosError);
    return iofs->io_DosError;
}
