#ifndef _APPOBJECTS_H_
#define _APPOBJECTS_H_

/*
    Copyright � 2011, The AROS Development Team. All rights reserved.
    $Id$
*/

#include <exec/types.h>
#include <workbench/workbench.h>

/*** Prototypes *************************************************************/
BOOL SendAppIconMessage(struct AppIcon * appicon, LONG numargs, STRPTR args);
APTR AppObjectsLock();
VOID AppObjectsUnlock(APTR lock);
struct AppIcon * GetNextAppIconLocked(struct AppIcon * lastappicon, APTR lock);
struct DiskObject * AppIcon_GetDiskObject(struct AppIcon * appicon);
CONST_STRPTR AppIcon_GetLabel(struct AppIcon * appicon);
BOOL AppIcon_Supports(struct AppIcon * appicon, ULONG tag); /* Takes one of the WBAPPICONA_SupportsXXX tags as parameter */
BOOL AppIcon_CallRenderHook(struct AppIcon * appicon, struct AppIconRenderMsg * msg);

#endif /* _APPOBJECTS_H_ */
