/*
    (C) 1997-1999 AROS - The Amiga Research OS
    $Id$

    Desc: Begining of AROS kernel
    Lang: English
*/

#define AROS_USE_OOP
#define AROS_ALMOST_COMPATIBLE 1

#include <intuition/intuition.h>

#include <exec/memory.h>

#include <proto/exec.h>
#include <graphics/gfx.h>
#include <utility/tagitem.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include <hidd/hidd.h>
#include <hidd/serial.h>

#define ioStd(x) ((struct IOStdReq *)x)

void hidd_demo()
{
    kprintf("graphics.hidd = %08.8lx\n",OpenLibrary("graphics.hidd",0));
    kprintf("vga.hidd = %08.8lx\n",OpenLibrary("vga.hidd",0));
    
    {
	struct GfxBase *GfxBase;
	BOOL success = FALSE;
    
        kprintf("init_gfx(hiddbase=%s)\n", "hidd.gfx.vga");
    
        GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 37);
        if (GfxBase)
        {

	    /*  Call private gfx.library call to init the HIDD.
	        Gfx library is responsable for closing the HIDD
	        library (although it will probably not be neccesary).
	    */

	    kprintf("calling private gfx LateGfxInit()\n");
	    if (LateGfxInit("hidd.gfx.vga"))
	    {
	        struct IntuitionBase *IntuitionBase;
	        kprintf("success\n");
			    
	        /* Now that gfx. is guaranteed to be up & working, let intuition open WB screen */
	        IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
	        if (IntuitionBase)
	        {
	    	    if (LateIntuiInit(NULL))
	    	    {
			success = TRUE;
		    }
		    CloseLibrary((struct Library *)IntuitionBase);
		}
	    }
	    kprintf("Closing gfx\n");
	
	    CloseLibrary((struct Library *)GfxBase);

	    if (success == FALSE)
	    {
	    	kprintf("There is something wrong with hidd subsystem...");
		while(1) {};
	    }
	
	}
    }

    {
	struct IORequest *io;
	struct MsgPort *mp;

	mp=CreateMsgPort();
	io=CreateIORequest(mp,sizeof(struct IOStdReq));
	kprintf("Result of opening device %d\n",
	    OpenDevice("gameport.device",0,io,0));
	kprintf("Doing CMD_HIDDINIT...\n");
	{
	    UBYTE *data;
	    data = AllocMem(100, MEMF_PUBLIC);
	    strcpy(data, "hidd.mouse.hw");
	    ioStd(io)->io_Command=32000;
	    ioStd(io)->io_Data=data;
	    ioStd(io)->io_Length=strlen(data);
	    DoIO(io);
	    kprintf("Got io_ERROR=%d",io->io_Error);
	}
    }

    {
        struct IntuitionBase *IntuitionBase;
        struct Window * win;
        int x = 100;
        int y = 100;


	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
	if (IntuitionBase)
	{
	    struct TagItem tags[] = {
		{WA_Width,			320},
		{WA_Height,			240},
		{WA_Left,			100},
		{WA_Top,			100},
		{WA_Title,  (ULONG)"AROS Dream :-)"},
		{WA_Activate,			  1},
		{WA_SizeGadget,                TRUE},
		{WA_DepthGadget,               TRUE},
		{TAG_DONE,			  0}};
	    win = OpenWindowTagList(0, tags);
	}

        DrawEllipse(win->RPort,160,120,80,80);
        DrawEllipse(win->RPort,185,90,15,15);
        DrawEllipse(win->RPort,135,90,15,15);
        
        Move(win->RPort,125,140);
        Draw(win->RPort,140,150);
        Draw(win->RPort,180,150);
        Draw(win->RPort,195,140);
        	
	if (win)
	{
	  while (x < 200)
	  {
	    MoveWindow(win,1,0);
	    x++;
	  }
	  
	  while (y < 200)
	  {
	    MoveWindow(win,0,1);
	    y++;
	  }
	  
	  while (x >= 100)
	  {
	    MoveWindow(win,-1,0);
	    x--;
	  }
	  
	  while (y >= 100)
	  {
	    MoveWindow(win,0,-1);
	    y--;
	  }
	}
    }
}