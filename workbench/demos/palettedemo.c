/*
    (C) 1995-97 AROS - The Amiga Replacement OS
    $Id$

    Desc: Demo/test for AROS boopsi objects
    Lang: english
*/

#include <aros/config.h>

#if !(AROS_FLAVOUR & AROS_FLAVOUR_NATIVE)
#   define ENABLE_RT 1
#endif

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <gadgets/arospalette.h>
#include <gadgets/aroscheckbox.h>
#include <intuition/classes.h>
#include <utility/tagitem.h>

#include <aros/rt.h>

#define SDEBUG 1
#define DEBUG 1
#include <aros/debug.h>

#define GID_PALETTE	1
#define GID_PROP	2

STATIC VOID HandleEvents(struct Window *);

struct IntuitionBase *IntuitionBase;
struct Library *AROSPaletteBase;

Object *palette, *prop;


/*************
**  main()  **
*************/
int main(int argc, char **argv)
{
    struct Task*inputDevice;
    
    EnterFunc(bug("main()\n"));
    
    RT_Init();
    
    /* Initialize the input.device's tc_UserData to 0 */
    inputDevice = FindTask("input.device");
    if (inputDevice)
    {
    	D(bug("Initializing input device's indent count\n"));
    	inputDevice->tc_UserData = NULL;
    }
    
    
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
    if (IntuitionBase)
    {
    	D(bug("Intuiton opened\n"));
 	
    	AROSPaletteBase = OpenLibrary(AROSPALETTECLASSPATH, 0);  
    	if (AROSPaletteBase)
    	{
    	    struct Screen *scr;

    	    D(bug("AROSPalette opened\n"));    	

    	    scr = LockPubScreen(NULL);
    	    if (scr)
    	    {
    	    	struct Window *window;

    	    	D(bug("Got screen %p\n", scr));

    	    	window = OpenWindowTags(NULL,
			 WA_PubScreen, scr,
			 WA_Left, 0,
			 WA_Top, 0,
			 WA_Width, 600,
			 WA_Height, 300,
			 WA_Title, "Try resize to another aspect ratio",
			 WA_IDCMP, 
			 	  IDCMP_GADGETUP 
			 	| IDCMP_MOUSEMOVE
			 	| IDCMP_MOUSEBUTTONS
			 	| IDCMP_REFRESHWINDOW
			 	| IDCMP_RAWKEY,
			 	
			 WA_SimpleRefresh, TRUE,
			 WA_DragBar, TRUE,
			 WA_CloseGadget, TRUE,
			 TAG_DONE);


    	    	if (window)
    	    	{
    		    D(bug("Window opened\n"));

    	    	    palette = NewObject(NULL, AROSPALETTECLASS,
    	    		GA_Left,		10,
    	    		GA_RelVerify,		TRUE,
    	    		GA_Top,			20,
    	    		GA_RelWidth,		-40,
    	    		GA_RelHeight,		-70,
    	    		GA_ID,			GID_PALETTE,
    	    		AROSA_Palette_Depth,	3,
    	    		TAG_DONE);
    	    		
		    prop = NewObject(NULL, PROPGCLASS,
		    	GA_RelVerify,	TRUE,
		    	GA_Left,	10,
		    	GA_RelBottom,	-40,
		    	GA_RelWidth,	-40,
		    	GA_Height,	30,
		    	GA_Previous,	palette,
		    	GA_ID,		GID_PROP,
		    	PGA_Total,	8,
		    	PGA_Visible,	1,
		    	PGA_Top,	2,
		    	PGA_Freedom,	FREEHORIZ,
		    	TAG_DONE);
		    	
		    	

    	    	    if (palette && prop)
    	    	    {
    	    	    	D(bug("Palette created\n"));
    	    	    	
    	    	    	D(bug("Adding Palette\n"));
    	    	    	AddGList(window, (struct Gadget *)palette, 0, 2, NULL);

    	    	    	D(bug("Refreshing Palette\n"));
    	    	    	RefreshGList((struct Gadget *)palette, window, NULL, 2);
    	    	    	
    	    		HandleEvents(window); 
    	    		
    	    		if (prop)
    	    		    DisposeObject(prop);
    	    		if (palette)
    	    		    DisposeObject(palette);
    	    		

    	    	    }
    	    	    CloseWindow(window);
    	    	} /* if (window opened) */

    	    	UnlockPubScreen(NULL, scr);
    	    } /*if (screen locked) */
    	    
    	    CloseLibrary(AROSPaletteBase);
    	}  /* if (paletteclass opened) */
    	
    	CloseLibrary((struct Library *)IntuitionBase);
    } /* if (intuition opened) */

    RT_Exit();
    
    ReturnInt ("main", int, 0);

}

/*******************
**  HandleEvents  **
*******************/
VOID HandleEvents(struct Window *win)
{
    struct IntuiMessage *imsg;
    struct MsgPort *port = win->UserPort;
    BOOL terminated = FALSE;
	
    EnterFunc(bug("HandleEvents(win=%p)\n", win));
    
    while (!terminated)
    {
	if ((imsg = (struct IntuiMessage *)GetMsg(port)) != NULL)
	{
	    
	    switch (imsg->Class)
	    {
		
	    case IDCMP_GADGETUP: {
	    
	    	Object *gad;
	    	
	        D(bug("Recieved gadgetup"));

	        gad = (Object *)imsg->IAddress;
	        
	        switch (((struct Gadget *)gad)->GadgetID)
	        {
	            case GID_PALETTE:
	            	D(bug("Color selected: %d\n", imsg->Code));
	            	break;
	            	
	            case GID_PROP:
	            	D(bug("Changed to depth %d\n", imsg->Code));
	            	/* Update the palette gadget accordingly */
	            	SetGadgetAttrs((struct Gadget *)palette, win, NULL,
	            		AROSA_Palette_Depth, imsg->Code + 1,
	            		TAG_DONE);
    	    	    	RefreshGList((struct Gadget *)palette, win, NULL, 1);
	            	break;
	        }
	    } break;
		
	    case IDCMP_REFRESHWINDOW:
	    	BeginRefresh(win);
	    	EndRefresh(win, TRUE);
	    	break;
	    	
	    case IDCMP_RAWKEY:
	    	terminated = TRUE;
	    	break;
		    					
	    } /* switch (imsg->Class) */
	    ReplyMsg((struct Message *)imsg);
	    
	    			
	} /* if ((imsg = GetMsg(port)) != NULL) */
	else
	{
	    Wait(1L << port->mp_SigBit);
	}
    } /* while (!terminated) */
	
    ReturnVoid("HandleEvents");
} /* HandleEvents() */


