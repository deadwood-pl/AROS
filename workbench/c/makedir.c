#include <exec/memory.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <utility/tagitem.h>

CALLENTRY /* Before the first symbol */

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;

static LONG tinymain(void);

LONG entry(struct ExecBase *sysbase)
{
    LONG error=RETURN_FAIL;
    SysBase=sysbase;
    DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",39);
    if(DOSBase!=NULL)
    {
	error=tinymain();
	CloseLibrary((struct Library *)DOSBase);
    }
    return error;
}

static LONG tinymain(void)
{
    STRPTR args[1]={ 0 };
    struct RDArgs *rda;
    LONG error=0;
    BPTR lock;

    rda=ReadArgs("DIR/A",(ULONG *)args,NULL);
    if(rda!=NULL)
    {
	lock = CreateDir(args[0]);

	if (lock)
	    UnLock(lock);
	else
	{
	    VPrintf ("Cannot create %s:", (ULONG *)args);
	    error = RETURN_FAIL;
	}

	FreeArgs(rda);
    }
    else
	error=RETURN_FAIL;

    if(error)
	PrintFault(IoErr(),"MakeDir");
    return error;
}
