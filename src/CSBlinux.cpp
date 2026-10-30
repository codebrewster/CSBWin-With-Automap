// CSBlinux.cpp : Linux-specific file instead of windows-specific CSBwin.cpp.
// Note: CSBTypes.h needs to be included after stdafx.h
// Keep imgui includes at the top to avoid the pragma pack in CSB.h
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "imguifilebrowser.h"
#include "stdafx.h"
#include "UI.h"
#include "resource.h"
#include <stdio.h>
#include <unistd.h>

#include "CSBTypes.h"
#include "confile.h"
#ifdef __APPLE__
#include "MacAppSupport.h"
#endif
#include "AutoMapHooks.h"
#include "AutoMapWindow.h"

#include "Dispatch.h"
#include "Objects.h"
#include "CSB.h"
#include "Data.h"

bool imgui_active = false;
static bool capturing_key;
static int captured_key;
char *opened_file;
char translation[10];

/**[Message List Stuff]*************************************/

#define MSG_LIST_SIZE	10	// maximum messages onscreen

typedef struct MESSAGE
{
    int color;
   char message[256];		// message string
} MESSAGE;

static struct MESSAGE MsgList[MSG_LIST_SIZE];

#define FONT_SIZE 14
#define DY (FONT_SIZE+6)

#define APPTITLE  "CSBwin"
#define APPVERSION  "15.7"
#define APPVERMINOR "v0" /* linux-only update*/

SDL_sem *sem;

static bool cursorIsShowing;
extern bool RecordMenuOption;
extern bool DMRulesDesignOption;
extern bool RecordDesignOption;
extern bool extendedPortraits;
bool fullscreenActive;
extern bool simpleEncipher; //not used in Linux
extern unsigned char *encipheredDataFile; //not used in Linux

static SDL_Event evert;
bool sdlQuitPending = false;
bool timerInQueue = false;

int absMouseX;  // screen resolution; you must divide by screenSize
int absMouseY;  //    to get position on ST screen.

#if defined SDL12
int screenWidth, screenHeight;
#endif


#if defined SDL20
SDL_Window *sdlWindow = NULL;
SDL_Texture *sdlTexture = NULL;
SDL_Renderer *sdlRenderer = NULL;
SDL_Rect desktop;
int SDL_VIDEOEXPOSE = 0;
#endif


#ifdef MAEMO_NOKIA_770
# include "hildon-widgets/hildon-program.h"
# include "hildon-widgets/hildon-window.h"

  HildonProgram *program = NULL;
  HildonWindow *hildonmainwindow = NULL;
#endif//MAEMO_NOKIA_770

ui32 TImER=0;

static void save_conf();

void PostQuitMessage( int a ) {
    if (UI_MessageBox(_("Save params ?"),"Quit",MESSAGE_YESNO) == MESSAGE_IDYES) {
	save_conf();
    }
    SDL_Event event;
//    FILE *f;
//    f = fopen("debug","a");
//    fprintf(f,"PostQuitMessage(0x%x)\n", a);
//    fclose(f);
    while (SDL_PollEvent(&event) != 0){};
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
    sdlQuitPending = true;
//    SDL_Quit ();
#ifdef USE_OLD_GTK
    gtk_exit (0);
#endif //USE_OLD_GTK
//    exit(0);
}

#define IDC_Timer 1

/*
static void DumpEvents(void)
{
  FILE *f;
  SDL_Event ev;
  f = fopen("debug","a");
  fprintf(f, "%s\n", SDL_GetError());
  while (SDL_PollEvent(&ev) != 0)
  {
    if (ev.type = SDL_USEREVENT)
    {
      fprintf(f, "SDL_USEREVENT type = %d\n", ev.user.code);
    }
    else
    {
      fprintf(f, "Event type= %d\n", ev.type);
    };
  };
  fclose(f);
}
*/

bool videoExposeWaiting = false;

static uint32_t __Timer_Callback(uint32_t interval, void *param)
{
  size_t code;
  code = (size_t)(param);
  if (code == IDC_Timer)
  {
      SDL_SemWait(sem);
    if(timerInQueue)
    {
      // We will add an IDC_Timer only when there is nothing
      // else to do.
	SDL_SemPost(sem);
      return interval;
    }
    else
    {
      // The queue is empty.  We can add an IDC_Timer and
      // (perhaps) an IDC_VIDEOEXPOSE.
      SDL_Event ev;
      if (videoExposeWaiting)
      {
        ev.type = SDL_USEREVENT;
        ev.user.code = IDC_VIDEOEXPOSE;
        if (SDL_PushEvent(&ev) < 0)
        {
//           DumpEvents();
          die(0x66a9);
        };
        videoExposeWaiting = false;
      }
      {
        ev.type = SDL_USEREVENT;
        ((SDL_UserEvent*) &ev)->code = IDC_Timer;
        if (SDL_PushEvent(&ev) < 0)
        {
          UI_MessageBox(SDL_GetError(), "PushEvent Failed",MESSAGE_OK);
          die(0x66aa);
        };
        timerInQueue = true;
        videoExposeWaiting = false;
      }
      SDL_SemPost(sem);
      return interval;
    }
  }
  else
  {
    PostQuitMessage(0xbab3);
  };
  return interval; // to avoid warning about not returning anything
}


static void PushEvent(void *param)
{
  SDL_Event ev;
  {
    size_t code;
    code = (size_t)(param);
    if (code == IDC_VIDEOEXPOSE)
    {
      if (SDL_PeepEvents(&ev,1,SDL_PEEKEVENT,
                     SDL_FIRSTEVENT,SDL_LASTEVENT) >0)
      {
        // We will add a IDC_VIDEOEXPOSE only when there is
        // nothing else to do.
        videoExposeWaiting = true;
        return;
      };
    };
  };
  ev.type = SDL_USEREVENT;
  ((SDL_UserEvent*) &ev)->code = (size_t)(param);
  if (SDL_PushEvent(&ev) < 0)
  {
    char line[80];
    sprintf(line," code = %zd", (size_t)(param));
    UI_MessageBox(SDL_GetError(), "PushEvent Failed",MESSAGE_OK);
    UI_Die(0x66ab);
  };
  return;
}

int WindowWidth = 320*4,winx,winy;
int WindowHeight = 200*4 + DY;
float st_X = 320.0 / WindowWidth;
float st_Y = 200.0 / (WindowHeight - 20);
static CSB_UI_MESSAGE csbMessage;

static void __resize_screen( ui32 w, i32 h ) {
#if 1
    if (w < 640 || h < 400+DY) {
	WindowWidth = 640;
	WindowHeight = 400+DY;
    } else {
	WindowWidth = w;
	WindowHeight = h;
    }
    st_X = 320.0/WindowWidth;
    st_Y = 200.0/WindowHeight;
    SDL_SetWindowSize(sdlWindow,WindowWidth,WindowHeight);
    csbMessage.type=UIM_REDRAW_ENTIRE_SCREEN;
    if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
    {
	PostQuitMessage(0x24);
    }
#else
  SDL_Event ev;
#if defined SDL12
  //ev.type = SDL_VIDEORESIZE;
  //((SDL_ResizeEvent*)&ev) -> w = w;
  //((SDL_ResizeEvent*)&ev) -> h = h;
#elif defined SDL20
  ev.type = IDC_VIDEORESIZE;
  ev.user.data1 = (void*)((size_t)w);
  ev.user.data2 = (void*)((size_t)h);
#else
  xxxError
#endif
  SDL_PushEvent(&ev);
#endif
}
/*
* There IS a videoexpose event in linux, but apparently SDL
* wants to handle those for itself... so we cannot use 'invalidate'
* the obvious way. UI_Invalidate just marks a request to
* update the screen... LIN_Invalidate is called from vbl and pretends
* to be the OS that grants the update.
*/

bool GetVideoRectangle(i32, RECT *);
static bool pending_update = true;

void UI_Invalidate(bool erase/*=false*/) {
  pending_update = true;
}
void LIN_Invalidate()
{// *screen, x, y, w, h
  if( pending_update ) {
    PushEvent((void*)(IDC_VIDEOEXPOSE) );
  }
}




#define MAX_LOADSTRING 100



i32 trace=-1;
#ifdef _DYN_WINSIZE
//i32 screenSize=2;
#else
//i32 screenSize=2;
#endif

extern bool BeginRecordOK;
extern bool ItemsRemain;
extern bool PlayfileIsOpen(void);
extern bool RecordfileIsOpen(void);
extern bool TimerTraceActive;
extern bool AttackTraceActive;
extern bool AITraceActive;
extern bool PlaybackCommandOption;
extern unsigned char *encipheredDataFile;// Oh?

extern i32 NoSpeedLimit;
extern i32 GameMode;
extern i32 MostRecentlyAdjustedSkills[2];
extern i32 LatestSkillValues[2];
extern XLATETYPE latestScanType;
extern XLATETYPE latestCharType;
extern i32 latestCharp1;
extern i32 latestScanp1;
extern i32 latestScanp2;
extern i32 latestScanXlate;
extern i32 latestCharXlate;
const char *helpMessage = "CSBlinux will need the orginal datafiles "
      "for either Chaos Strikes Back,\n"
      "or Dungeon Master in your current working directory.\n"
      "If the files are located somewhere else try\n --help for some usefull comandlinde arguments.\n"
      "\nMake sure that you have these files\n"
      "(and that they are spelled with lower case)\n"
      "   dungeon.dat\n"
      "   hcsb.dat\n"
      "   hcsb.hct\n"
      "   mini.dat\n"
      "   graphics.dat\n"
      "   csb.cfg";

/* This is a bad place for these id's */

void MTRACE(const char *msg)
{
  if (trace < 0) return;
  FILE *f = GETFILE(trace);
  fprintf(f, "%s", msg);
  fflush(f);
}

void UI_GetCursorPos(i32 *_x, i32 *_y)
{
  *_x = X_TO_CSB(absMouseX,screenSize);
  *_y = Y_TO_CSB(absMouseY,screenSize);
}



char szCSBVersion[] = "CSB Version " __DATE__;
int WindowX = 0;
int WindowY = 0;
bool fullscreenRequested = false;


i32 line = 0;

// Global Variables:
SDL_Surface *WND = NULL; //defined in UI.h
SDL_TimerID timer;

#ifdef _DYN_WINSIZE
HermesHandle from_palette;
HermesHandle to_palette;
HermesHandle converter;
HermesFormat* from_format;
#else
SDL_Surface *SCRAP = NULL;
#endif

void done_imgui() {
    // Cleanup
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

/* Free the global allocated data and finish the main loop */
void cbAppDestroy(void)
{
  /* Close SDL (prevents the ticks to continue running) */
  //printf("\nQuitting...\n");
    Cleanup(true);
    done_imgui();
    doneCaches();
    SDL_RemoveTimer(timer);
    SDL_DestroySemaphore(sem);
    sem = NULL;
  SDL_Quit ();
#ifdef USE_OLD_GTK
  /* End with the GTK*/
  gtk_exit (0);
#endif //USE_OLD_GTK
    exit(0);
}

void g_log(const char *, int, const char *, ...)
{
    die(0x3512);
}

extern bool resetWhatToDo,resetgamesetup,resetprisondoor,resettag1,resettag2;

void Process_SDL_MOUSEMOTION(
           bool& cursorIsShowing)
{
  MTRACE("SDL_MOUSEMOTION\n");
  SDL_MouseMotionEvent *e = (SDL_MouseMotionEvent*) &evert;
  int x, y;
  bool warp = false;
  x = e->x * st_X;
  y = (e->y - DY) * st_Y;
  if (y < 0) y = 0;
#if 0
  // I don't see the reason of this for now
  if (x >= 316*screenSize) {x = 316*screenSize-1; warp=true;};
  if (y >= 196*screenSize) {y = 196*screenSize-1; warp=true;}
  if (x <    1*screenSize) {x =   1*screenSize;   warp=true;}
  if (y <    1*screenSize) {y =   1*screenSize;   warp=true;}
#endif
  if (warp)
  {
#ifdef SDL12
    SDL_WarpMouse(x, y);
#elif defined SDL20
//    SDL_WarpMouseGlobal(x, y);
#else
    xxError
#endif
    return;
  };
  absMouseX = x;
  absMouseY = y;
  if (resetgamesetup) return;
  if (GameMode == 1)
  {
    if (cursorIsShowing && !imgui_active)
    {
      cursorIsShowing = false;
      SDL_ShowCursor(SDL_DISABLE);
    };
  }
  else
  {
    if (!cursorIsShowing)
    {
      cursorIsShowing = true;
      SDL_ShowCursor(SDL_ENABLE);
    };
  };
//  {
//    FILE *f;
//    static int prevX, prevY;
//    if ((e->x != prevX) || (e->y != prevY))
//    {
//      f = fopen("/run/shm/debug","a");
//      fprintf(f, "Movement e->x=%d(%d), e->y=%d(%d); set mouseX=%d mouseY=%d showing=%d\n",
//                  e->x, e->xrel, e->y, e->yrel, absMouseX, absMouseY, cursorIsShowing);
//      fclose(f);
//    };
//  };
}

void Process_ecode_IDC_Normal(void)
{
  MTRACE("IDC_Normal\n");
  __resize_screen( 320, 240 );
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_NORMAL;
  csbMessage.p2 = 2; //2-(screenSize==1); //new value
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(1);
  };
}

void Process_ecode_IDC_Timer(void)
{
  MTRACE("IDC_Timer\n");
  if(GameMode != 1)
  {
    LIN_Invalidate();
  }
  SDL_SemWait(sem);
  timerInQueue = false;
  SDL_SemPost(sem);
       //One-at-a-time, please
       //The problem was that under certain circumstances too
       //many timer events get queued and the SDL queue gets
       //full and keystrokes get discarded!
  csbMessage.type=UIM_TIMER;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(2);
  };
}

void Process_ecode_IDC_VIDEOEXPOSE(void)
{
  /*
   * BEGIN the evil VIDEOEXPOSE
   */
  MTRACE("IDC_VIDEOEXPOSE\n");
  //printf("IDC_VIDEOEXPOSE\n");
  line++;
  line = line%10;
  //        memset( SCR, line, 40 );
  //        SDL_BlitSurface(SCR, NULL, WND, NULL );
  switch(line)
  {
    case 0:
      //        g_print("vblint = %u",VBLInterruptCount);
      break;
    case 1:
      //        g_print("chkvbl = %u",CheckVBLCount);
      break;
    case 2:
    //        g_print("STblt = %u",STBLTCount);
      break;
    case 3:
    //  g_print("Time = %u",GameTime);
      break;
    case 4:
    //  g_print(
    //         "Skill[%d]=0x%08x      ",
    //          MostRecentlyAdjustedSkills[0],
    //          LatestSkillValues[0]);
      break;
    case 5:
    //  g_print(
    //          "Skill[%d]=0x%08x      ",
    //          MostRecentlyAdjustedSkills[1],
    //          LatestSkillValues[1]);
      break;
    case 6:
      break;
    case 7:
      switch (latestScanType)
      {
        case TYPESCAN:
        case TYPEIGNORED:
    //      g_print( "%08x mscan --> Ignored                        ",latestScanp1);
          break;
        case TYPEMSCANL:
    //      g_print( "%08x mscan --> Translated to %08x L",latestScanp1,latestScanXlate);
          break;
        case TYPEMSCANR:
    //      g_print( "%08x mscan --> Translated to %08x R",latestScanp1,latestScanXlate);
          break;
        default:
    //      g_print("                                              ");
          break;
      };
      break;
    case 8:
      switch (latestScanType)
      {
        case TYPEMSCANL:
        case TYPEIGNORED:
        case TYPEMSCANR:
    //      g_print( "%08x scan --> Ignored                        ",latestScanp2);
          break;
        case TYPESCAN:
    //      g_print( "%08x scan --> Translated to %08x", latestScanp2,latestScanXlate);
          break;
        default:
    //      g_print("                                              ");
          break;
      };
      break;
    case 9:
    //  g_print("Total Moves = %d",totalMoveCount);
      break;
  };
  //TextOut(hdc,325,25+15*line,msg,strlen(msg));
  //        g_warning(msg);
  /*
   * This message should be sent when the screen has been erased?
   *
   */
  /*
    csbMessage.type=UIM_REDRAW_ENTIRE_SCREEN;
    if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
    {
      PostQuitMessage(0);
      break;
    };
  */

  /*
   * Finally, write to the screen!
   */
  csbMessage.type=UIM_PAINT;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(3);
  };
}

void Process_ecode_IDC_Double(void)
{
  MTRACE("IDC_Double\n");
  __resize_screen( 320*2, 200*2);
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_DOUBLE;
  csbMessage.p2 = 2; //2-(screenSize==2); //new value
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(4);
  };
}

void Process_ecode_IDC_Triple(void)
{
  MTRACE("IDC_Triple\n");
  __resize_screen( 320*3, 200*3 );
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_TRIPLE;
  csbMessage.p2 = 2; //2-(screenSize==3); //new value
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(5);
  };
}

void Process_ecode_IDC_Quadruple(void)
{
  MTRACE("IDC_Quadruple\n");
  __resize_screen( 320*4, 200*4 );
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_QUADRUPLE;
  csbMessage.p2 = 2; //2-(screenSize==4); //new value
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(6);
  };
}

void Process_ecode_IDC_Quintuple(void)
{
  MTRACE("IDC_Quintuple\n");
  __resize_screen( 320*5, 200*5 );
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_QUINTUPLE;
  csbMessage.p2 = 2; //2-(screenSize==4); //new value
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(7);
  };
}

void Process_ecode_IDC_Sextuple(void)
{
  MTRACE("IDC_Sextuple\n");
  __resize_screen( 320*6, 200*6 );
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_SEXTUPLE;
  csbMessage.p2 = 2; //2-(screenSize==4); //new value
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(8);
  };
}

void Process_ecode_IDC_QuickPlay(void)
{
  MTRACE("IDC_QuickPlay\n");
  if (!PlayfileIsOpen()) return;
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_QUICKPLAY;
  csbMessage.p2 = (NoSpeedLimit!=0) ? 0 : 1;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(9);
  };
}

void Process_ecode_IDM_Glacial(void)
{
  MTRACE("IDM_Glacial\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_CLOCK;
  csbMessage.p2 = SPEED_GLACIAL;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0xa);
  };
}

void Process_ecode_IDM_Molasses(void)
{
  MTRACE("IDM_Molasses\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_CLOCK;
  csbMessage.p2 = SPEED_MOLASSES;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0xb);
  };
}

void Process_ecode_IDM_VerySlow(void)
{
  MTRACE("IDM_VerySlow\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_CLOCK;
  csbMessage.p2 = SPEED_VERYSLOW;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0xc);
  };
}

void Process_ecode_IDM_Slow(void)
{
  MTRACE("IDM_Slow\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_CLOCK;
  csbMessage.p2 = SPEED_SLOW;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0xd);
  };
}

void Process_ecode_IDM_Normal(void)
{
  MTRACE("IDM_Normal\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_CLOCK;
  csbMessage.p2 = SPEED_NORMAL;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0xe);
  };
}

void Process_ecode_IDM_Fast(void)
{
  MTRACE("IDM_Fast\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_CLOCK;
  csbMessage.p2 = SPEED_FAST;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0xf);
  };
}

void Process_ecode_IDM_Quick(void)
{
  MTRACE("IDM_Quick\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_CLOCK;
  csbMessage.p2 = SPEED_QUICK;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x10);
  };
}

void Process_ecode_IDM_PlayerClock(void)
{
  MTRACE("IDM_PlayerClock\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_PLAYERCLOCK;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x11);
  };
}

void Process_ecode_IDM_ExtraTicks(void)
{
  MTRACE("IDM_ExtraTicks\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_EXTRATICKS;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x12);
  };
}

void Process_ecode_IDC_Record(void)
{
  MTRACE("IDC_Record\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_RECORD;
  csbMessage.p2 = 1;
  if (RecordMenuOption) csbMessage.p2 = 0;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x13);
  };
}

void Process_ecode_IDC_TimerTrace(void)
{
  MTRACE("IDC_TimerTrace\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_TIMERTRACE;
  csbMessage.p2 = 1;
  if (TimerTraceActive) csbMessage.p2 = 0;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x14);
  };
}

void Process_ecode_IDM_DMRules(void)
{
   MTRACE("IDM_DMRULES\n");
   csbMessage.type = UIM_SETOPTION;
   csbMessage.p1 = OPT_DMRULES;
   if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
   {
     PostQuitMessage(0x15);
   };
}

void Process_ecode_IDC_AttackTrace(void)
{
  MTRACE("IDC_AttackTrace\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_ATTACKTRACE;
  csbMessage.p2 = 1;
  if (AttackTraceActive) csbMessage.p2 = 0;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x16);
  };
}

void Process_ecode_IDC_AITrace(void)
{
  MTRACE("IDC_AITrace\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_AITRACE;
  csbMessage.p2 = 1;
  if (AITraceActive) csbMessage.p2 = 0;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x17);
  };
}

void Process_ecode_IDC_ItemsRemaining(void)
{
  MTRACE("IDC_ItemsRemaining\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_ITEMSREMAINING;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x18);
  };
}

void Process_ecode_IDC_NonCSBItemsRemaining(void)
{
  MTRACE("IDC_NonCSBItemsRemaining\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_NONCSBITEMSREMAINING;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x19);
  };
}

static void Process_ecode_IDC_Playback(void)
{
  MTRACE("IDC_Playback\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_PLAYBACK;
  csbMessage.p2 = 1;
  if (PlayfileIsOpen()) csbMessage.p2 = 0;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x1a);
  };
}

static void Process_ecode_IDC_DispatchTrace(void)
{
  MTRACE("IDC_DispatchTrace\n");
  if (trace  >= 0 )
  {
    CLOSE(trace);
    trace = -1;
  }
  else
  {
    trace = CREATE("trace.log","w", true);
  };
}

static void Process_ecode_IDM_GraphicTrace(void)
{
  MTRACE("IDC_GraphicTrace\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_GRAPHICTRACE;
  csbMessage.p2 = 1;
  if (TimerTraceActive) csbMessage.p2 = 0;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x1b);
  };
}

static void Process_ecode_IDC_DSATrace(void)
{
  MTRACE("IDC_DSATrace\n");
  csbMessage.type = UIM_SETOPTION;
  csbMessage.p1 = OPT_DSATRACE;
  csbMessage.p2 = 1;
  if (DSATraceActive) csbMessage.p2 = 0;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x1c);
  };
}

static void Process_SDL_USEREVENT(void)
{
  MTRACE("sdl_userevent->");
  SDL_UserEvent *e = (SDL_UserEvent*) &evert;
  switch( e->code )
  {// the menu (and timer)
    case IDC_Normal:        Process_ecode_IDC_Normal();        break;
    case IDC_Timer:         Process_ecode_IDC_Timer();         break;
    case IDC_VIDEOEXPOSE:   Process_ecode_IDC_VIDEOEXPOSE();   break;
    case IDC_Double:        Process_ecode_IDC_Double();        break;
    case IDC_Triple:        Process_ecode_IDC_Triple();        break;
    case IDC_Quadruple:     Process_ecode_IDC_Quadruple();     break;
    case IDC_Quintuple:     Process_ecode_IDC_Quintuple();     break;
    case IDC_Sextuple:      Process_ecode_IDC_Sextuple();      break;
    case IDC_QuickPlay:     Process_ecode_IDC_QuickPlay();     break;
    case IDM_Glacial:       Process_ecode_IDM_Glacial();       break;
    case IDM_Molasses:      Process_ecode_IDM_Molasses();      break;
    case IDM_VerySlow:      Process_ecode_IDM_VerySlow();      break;
    case IDM_Slow:          Process_ecode_IDM_Slow();          break;
    case IDM_Normal:        Process_ecode_IDM_Normal();        break;
    case IDM_Fast:          Process_ecode_IDM_Fast();          break;
    case IDM_Quick:         Process_ecode_IDM_Quick();         break;
    case IDM_PlayerClock:   Process_ecode_IDM_PlayerClock();   break;
    case IDM_ExtraTicks:    Process_ecode_IDM_ExtraTicks();    break;
    case IDC_Record:        Process_ecode_IDC_Record();        break;
    case IDC_TimerTrace:    Process_ecode_IDC_TimerTrace();    break;
    case IDM_DMRULES:       Process_ecode_IDM_DMRules();       break;
    case IDC_AttackTrace:   Process_ecode_IDC_AttackTrace();   break;
    case IDC_AITrace:       Process_ecode_IDC_AITrace();       break;
    case IDC_ItemsRemaining:Process_ecode_IDC_ItemsRemaining();break;
    case IDC_Playback:      Process_ecode_IDC_Playback();      break;
    case IDC_DispatchTrace: Process_ecode_IDC_DispatchTrace(); break;
    case IDM_GraphicTrace:  Process_ecode_IDM_GraphicTrace();  break;
    case IDC_DSATrace:      Process_ecode_IDC_DSATrace();      break;
    case IDC_NonCSBItemsRemaining:
                            Process_ecode_IDC_NonCSBItemsRemaining();
                                                               break;
    default:
      printf("Unknown SDL_USEREVENT\n");
      MTRACE("Unknown SDL_USEREVENT\n");
      //return DefWindowProc(hWnd, message, wParam, lParam);
      break;
  };
}

static void stop_capture(int);

static void Process_SDL_MOUSEBUTTONDOWN(void)
{
  MTRACE("SDL_MOUSEBUTTONDOWN->");
  {
    SDL_MouseButtonEvent  *e = (SDL_MouseButtonEvent*) &evert;
    if (capturing_key) {
	stop_capture(e->button);
	return;
    }
    switch(e->button)
    {// btn down, left or right
      case SDL_BUTTON_LEFT:
        MTRACE("SDL_BUTTON_LEFT\n");
        //printf("left button @%d ", (ui32)UI_GetSystemTime());
        csbMessage.type=UIM_LEFT_BUTTON_DOWN;
        csbMessage.p1 = absMouseX;
        csbMessage.p2 = absMouseY;
        if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
        {
          PostQuitMessage(0x1d);
        };
        break;
      case SDL_BUTTON_RIGHT:
        MTRACE("SDL_BUTTON_RIGHT\n");
        //printf("right ");
        csbMessage.type=UIM_RIGHT_BUTTON_DOWN;
        csbMessage.p1 = absMouseX; // = e->x;  // horizontal position of cursor
        csbMessage.p2 = absMouseY; // = e->y;    // vertical position of cursor
        if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
        {
          PostQuitMessage(0x1e);
        };
        break;
    }
  }
  //printf("at (%x, %x)\n", X_TO_CSB(mouseX,screenSize),Y_TO_CSB(mouseY,screenSize));
}

static void Process_SDL_MOUSEBUTTONUP(void)
{
  MTRACE("SDL_MOUSEBUTTONUP->");
  {// btn up, left or right
    SDL_MouseButtonEvent  *e = (SDL_MouseButtonEvent*) &evert;
    switch(e->button)
    {
      case SDL_BUTTON_RIGHT:
        MTRACE("SDL_BUTTON_RIGHT\n");
        csbMessage.type=UIM_RIGHT_BUTTON_UP;
        csbMessage.p1 = absMouseX; // = e->x;  // horizontal position of cursor
        csbMessage.p2 = absMouseY; // = e->y; // vertical position of cursor
        if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
        {
          PostQuitMessage(0x1f);
        };
        break;
      case SDL_BUTTON_LEFT:
        MTRACE("SDL_BUTTON_LEFT\n");
        csbMessage.type=UIM_LEFT_BUTTON_UP;
        csbMessage.p1 = absMouseX; // = e->x;   // horizontal position of cursor
        csbMessage.p2 = absMouseY; // = e->y;  // vertical position of cursor
        if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
        {
          PostQuitMessage(0x20);
        };
        break;
    };
  };
}

static void Process_SDL_KEYDOWN(void)
{
//  static int prevp1 = -1;
  MTRACE("SDL_KEYDOWN\n");
  {
    /*SDL_KeyboardEvent *e = (SDL_KeyboardEvent*) &evert;
    if(e->keysym->sym==SDLK_LEFT)
    printf("left ");
    if else(e->keysym->sym==SDLK_RIGHT)
    printf("right ");
    */
    csbMessage.type=UIM_KEYDOWN;
    csbMessage.p1 =  evert.key.keysym.sym; //virtual key
    csbMessage.p2 = evert.key.keysym.scancode; //scancode
    if (capturing_key) {
	captured_key = csbMessage.p2;
	return;
    }
    // The map's level keys: with Ctrl always, plain only when the Keys window has not bound
    // them (the default csb.cfg makes Page Up and Page Down pick up items)
    bool keyIsBound = keyxlate.translate(csbMessage.p2, TYPESCAN) != 0
	|| keyxlate.translate(csbMessage.p2, TYPEMSCANL) != 0
	|| keyxlate.translate(csbMessage.p2, TYPEMSCANR) != 0;
    if ((!keyIsBound || (evert.key.keysym.mod & KMOD_CTRL))
	&& AutoMapWindow_HandleLevelKey(evert.key.keysym.sym)) {
	AutoMapWindow_PaintIfDirty();
	return;
    }
    //printf("key press %x %x mod %x\n",csbMessage.p1,csbMessage.p2,evert.key.keysym.mod);
    //printf("Key 0x%x pressed @%d\n",(int)csbMessage.p1, (ui32)UI_GetSystemTime());
//    if ((csbMessage.p1 == 3) && (prevp1 == 3))
//    {
//      PostQuitMessage(0);
//      return;
//    };
//    prevp1 = csbMessage.p1;
    if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
    {
      PostQuitMessage(0x21);
    }
  }
  {
    static int prevKeyUp = -1;
    csbMessage.type=UIM_CHAR;
    csbMessage.p1 = evert.key.keysym.sym;
    if (evert.key.keysym.sym == SDLK_LCTRL) return;
    if (evert.key.keysym.sym == SDLK_RCTRL) return;
    //{
    //  char line[80];
    //  sprintf(line, "p1=%d; mod=%d", csbMessage.p1, evert.key.keysym.mod);
    //  UI_MessageBox(line, "KEYUP", MESSAGE_OK);
    //};
    if (evert.key.keysym.mod &  (KMOD_LCTRL | KMOD_RCTRL))
    {
      if (csbMessage.p1 == SDLK_c)
      {
        if (prevKeyUp == 3)
        {
          PostQuitMessage(0x22);
          return;
        };
        csbMessage.p1 = 3; //Control-c
      };
    };
#if defined SDL20
    if ((evert.key.keysym.mod & (KMOD_ALT)) && evert.key.keysym.sym == SDLK_RETURN) {
	SDL_DisplayMode mode;
	SDL_GetDesktopDisplayMode(0,&mode);

	if (fullscreenActive) {
	    fullscreenActive = false;
	} else {
	    fullscreenActive = true;
	    winx = WindowWidth;
	    winy = WindowHeight;
	    SDL_SetWindowDisplayMode(sdlWindow,&mode);
	}
	SDL_SetWindowFullscreen(sdlWindow,(fullscreenActive ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    }
#endif

    prevKeyUp = csbMessage.p1;
    if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
    {
      PostQuitMessage(0x23);
    }
  }
}

static void Process_SDL_KEYUP(void)
{
  MTRACE("SDL_KEYUP\n");
}

#ifdef SDL20

void Process_SDL_WINDOWEVENT(const SDL_Event &event)
{
  if (AutoMapWindow_HandleWindowEvent(event)) return;
  switch(event.window.event)
  {
    case SDL_WINDOWEVENT_SHOWN:
      break;
    case SDL_WINDOWEVENT_HIDDEN:
      break;
    case SDL_WINDOWEVENT_LEAVE:
      break;
    case SDL_WINDOWEVENT_MOVED:
      break;
    case SDL_WINDOWEVENT_SIZE_CHANGED:
      break;
    case SDL_WINDOWEVENT_RESIZED:
      __resize_screen( event.window.data1, event.window.data2 );
      break;
    case SDL_WINDOWEVENT_EXPOSED:
      csbMessage.type=UIM_REDRAW_ENTIRE_SCREEN;
      if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
      {
	  PostQuitMessage(0x24);
      }
      break;
    case SDL_WINDOWEVENT_ENTER:
      break;
    case SDL_WINDOWEVENT_FOCUS_GAINED:
      break;
    case SDL_WINDOWEVENT_FOCUS_LOST:
      break;
    case SDL_WINDOWEVENT_MINIMIZED:
      break;
    case SDL_WINDOWEVENT_MAXIMIZED:
      break;
    case SDL_WINDOWEVENT_RESTORED:
      break;
    case SDL_WINDOWEVENT_TAKE_FOCUS:
      break;
    case SDL_WINDOWEVENT_HIT_TEST:
      break;
#if SDL_VERSION_ATLEAST(2,0,18)
    // macOS sends both of these as soon as the window appears on a display
    case SDL_WINDOWEVENT_ICCPROF_CHANGED:
      break;
    case SDL_WINDOWEVENT_DISPLAY_CHANGED:
      break;
#endif
    case SDL_WINDOWEVENT_CLOSE:
      PostQuitMessage(1);
      break;
    default:
      {
        char line[80];
        sprintf(line,
                "event.window.event = %d\n",
                event.window.event);
        UI_MessageBox(line,
                      "Process_SDL_WINDOWEVENT",
                      MESSAGE_OK);
        NotImplemented(0x66); break;
        // check for RESIZE
    };
  };
}

#endif
ImGuiIO io;
static int drawn = 0;
void post_render();

// The share of an event that the menu bar and the map window take, for loops that keep the
// game itself waiting (the end sequence after a win): the game gets nothing of it
void ProcessMenuEvent(const SDL_Event &event)
{
  if (AutoMapWindow_HandlePointerEvent(event)) return;
  if ((event.type == SDL_KEYDOWN || event.type == SDL_TEXTINPUT) && AutoMapWindow_HandleKeyEvent(event)) return;
  ImGui_ImplSDL2_ProcessEvent(&event);
}

// The game hides the system pointer because it draws its own. The map window has no drawn
// pointer, so the system pointer stays while it is over the map.
static void hide_system_pointer() {
    if (AutoMapWindow_PointerIsOverMap()) return;
    SDL_ShowCursor(SDL_DISABLE);
}

typedef struct {
    const char *desc;
    int key;
    int key1, key2;
    bool active1,active2;
} TDescKeys;

static TDescKeys mykeys[] = {
    { "Pause", 0x1b },
    { "Forward", 0x480000 },
    { "Turn Right", 0x470000 },
    { "Turn Left", 0x520000 },
    { "Move right", 0x4d0000 },
    { "Move Left", 0x4b0000 },
    { "Move back", 0x500000 },
    { NULL, 0 }
};

static TDescKeys mkeys[] = { // Mouse to keyboard inputs...
    { "1st char 1st attack", 0xf0005c },
    { "2nd char 1st attack", 0x104005c },
    { "3rd char 1st attack", 0x11c005c },
    { "4th char 1st attack", 0x138005c },

    { "1st char 2nd attack", 0xf00068 },
    { "2nd char 2nd attack", 0x1040068 },
    { "3rd char 2nd attack", 0x11c0068 },
    { "4th char 2nd attack", 0x1380068 },

    { "1st char 3rd attack", 0xf00074 },
    { "2nd char 3rd attack", 0x1040074 },
    { "3rd char 3rd attack", 0x11c0074 },
    { "4th char 3rd attack", 0x1380074 },

    { "1st rune", 0xf00037 },
    { "2nd rune", 0xfe0037 },
    { "3nd rune", 0x10a0037 },
    { "4th rune", 0x11d0037 },
    { "5th rune", 0x1280037 },
    { "6th rune", 0x1360037 },
    { "backspace rune", 0x1360043 },
    { "cast spell", 0x10d0041 },
    { "1st magic caster", 0x200002e },
    { "2nd magic caster", 0x201002e },
    { "3rd magic caster", 0x202002e },
    { "4th magic caster", 0x203002e },

    { "No operation", 0x114002f },
    { "Pass (cancel attack)", 0x1310050 },

    { "Pick up left near", 0x43009d },
    { "Pick up right near", 0x9a009f },
    { "Pick up left far", 0x4d008c },
    { "Pick up right far", 0x96008c },
    { "Pick up alcove", 0x6b0073 },
    { "Door switch", 0xa90051 },
    { "Throw Left", 0x6f0050 },
    { "Throw Right", 0x93004f },
    { NULL, 0},
};

/********************** MAIN **********************/
static void read_conf();

static ImFont* add_font(char *font) {
  FILE *f = fopen(font,"rb");
  if (f) {
      fclose(f);
      ImFontConfig cfg;
      cfg.OversampleH = 1;
      static ImVector<ImWchar> gr;
      gr.clear();
      static ImFontGlyphRangesBuilder range;
      range.Clear();
      range.AddRanges(io.Fonts->GetGlyphRangesJapanese());
      range.BuildRanges(&gr);
      return io.Fonts->AddFontFromFileTTF(font, FONT_SIZE,&cfg,gr.Data);
  }
  return NULL;
}

static ImFont *wall_font,*scroll_font;

int main(int argc, char* argv[])
{
#ifdef MAEMO_NOKIA_770
  HildonProgram* program;
#endif//MAEMO_NOKIA_770
  /* Set default 'root' directory. */
  //folderSavedGame=(char*)".";
//  {
//    FILE *f;
//    f = fopen("/run/shm/debug", "w");
//    fprintf(f,"Debugging info\n");
//    fclose(f);
//  };
  folderParentName=(char*)".";
  folderName=(char*)".";
  root = (char*)".";
#ifdef __APPLE__
  UseApplicationSupportFolderWhenBundled();
#endif
  /* Parse commandline arguments.
   * Eat all arguments I recognize. Otherwise Gnome kills me.
   */
  printf("%s\n", APPTITLE  " " APPVERSION APPVERMINOR );
        versionSignature = Signature(szCSBVersion);
  if( UI_ProcessOption(argv,argc) == false )
  {
      exit(0);
  }
  if( !(folderParentName&&folderName&&root) )
  {
    root=(char*)"./";
  }
  read_conf();

  //    ***** Init the Aplication enviroment ******

#ifdef USE_OLD_GTK
  gtk_init (&argc, &argv);
  appGlobal = gtk_window_new (GTK_WINDOW_TOPLEVEL);

#ifdef MAEMO_NOKIA_770
  /* Create the hildon application and setup the title */
  program = HILDON_PROGRAM ( hildon_program_get_instance () );
  hildonmainwindow = HILDON_WINDOW(hildon_window_new());
  g_set_application_name ( "CSB" );
  //    g_signal_connect(G_OBJECT(hildonmainwindow), "delete_event", gtk_main_quit, NULL);
  hildon_program_add_window(program, hildonmainwindow);

  appGlobal = GTK_WIDGET(hildonmainwindow); //gtk_window_new (GTK_WINDOW_TOPLEVEL);
#endif//MAEMO_NOKIA_770

  if (appGlobal == NULL)
  {
    g_error("main: GTK application is a nullpointer.");
  }
  gtk_window_set_title (GTK_WINDOW (appGlobal), APPTITLE);
  //gtk_signal_connect (GTK_OBJECT (appGlobal), "delete_event",GTK_SIGNAL_FUNC (cbAppDestroy), NULL);
  gtk_signal_connect (GTK_OBJECT (appGlobal), "destroy",GTK_SIGNAL_FUNC (cbAppDestroy), NULL);
  gtk_signal_connect (GTK_OBJECT (appGlobal), "expose_event",GTK_SIGNAL_FUNC (__timer_callback),(void*)( IDC_VIDEOEXPOSE));

  //gtk_create_menu
  {
    GtkItemFactory *item_factory;
    GtkAccelGroup *accel_group;
    GtkWidget *menuW;
    GtkWidget *main_vbox;
    static const gint nmenu_items = sizeof (menubar) / sizeof (menubar[0]);

    accel_group = gtk_accel_group_new ();
    item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>",
                                       accel_group);
    gtk_item_factory_create_items (item_factory, nmenu_items, menubar, NULL);
    gtk_window_add_accel_group (GTK_WINDOW (appGlobal), accel_group);
    gtk_signal_connect(GTK_OBJECT(gtk_item_factory_get_widget(item_factory,"/Misc")),
                       "expose_event",
    GTK_SIGNAL_FUNC(__before_misc_menu_is_showed),
                     (gpointer) item_factory);
    gtk_signal_connect(GTK_OBJECT(gtk_item_factory_get_widget(item_factory,"/Trace")),
                      "expose_event",
    GTK_SIGNAL_FUNC(__before_trace_menu_is_showed),
                    (gpointer) item_factory);
    menuW = gtk_item_factory_get_widget (item_factory, "<main>");
    main_vbox = gtk_vbox_new (FALSE, 1);
    gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 1);
    gtk_container_add (GTK_CONTAINER (appGlobal), main_vbox);
    gtk_box_pack_start (GTK_BOX (main_vbox), menuW, FALSE, TRUE, 0);

    gtk_check_menu_item_set_active  (GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(item_factory,"/Misc/Size x 2")), true);// Default is '2 x ScreenSize'
    gtk_check_menu_item_set_active  (GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(item_factory,"/Speed/Normal")), true); // Default is 'Normal' speed
    if(!PlaybackCommandOption)
    {
      gtk_check_menu_item_set_active  (GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(item_factory,"/Speed/Extra Ticks")), true);extraTicks=true;// Default is 'Extra Ticks'
    };
  };


  gtk_widget_show_all (appGlobal);
#ifdef MAEMO_NOKIA_770
  /* Hack to get SDL to use GTK window */
  /* Doesn't work with gtk-1.2 */
  {
    char SDL_windowhack[32];
    sprintf( SDL_windowhack,"SDL_WINDOWID=%ld",
            (GTK_WINDOW(hildonmainwindow)));
            (GTK_WINDOW(appGlobal)));

    //      GDK_WINDOW_XWINDOW(appGlobal->window));
    putenv(SDL_windowhack);
    printf("Done SDL window hack. ID should be %ld\n%s", GDK_WINDOW_XWINDOW(appGlobal->window), SDL_windowhack);
  };
#endif //MAEMO_NOKIA_770
#endif //USE_OLD_GTK

//    ***** Initialize defaults, Video and Audio *****
  if ( SDL_Init ( SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER|SDL_INIT_EVENTS)<0)//|SDL_INIT_AUDIO|SDL_INIT_TIMER) < 0)
  // if ( SDL_Init ( SDL_INIT_VIDEO)<0)//|SDL_INIT_AUDIO|SDL_INIT_TIMER) < 0)
  {
    fprintf(stderr,"Unable to init SDL: %s", SDL_GetError() );
    //g_error("Unable to init SDL: %s", SDL_GetError() );
  };
  printf("SDL initialized.\n");
#if defined SDL20
  SDL_SetWindowTitle(sdlWindow,szCSBVersion);
#else
  xxxError
#endif


// ****************************** the DISPLAY ****************************************************
  {
#if defined SDL20
    int flags;
    screenSize = 1;
    flags = 0;
    if (fullscreenRequested)
    {
      flags =  SDL_WINDOW_FULLSCREEN_DESKTOP;
    };
    flags |= SDL_WINDOW_RESIZABLE;
    SDL_GetDisplayBounds(0,&desktop);
    if ((sdlWindow = SDL_CreateWindow(
                   szCSBVersion,
                   SDL_WINDOWPOS_CENTERED,
                   SDL_WINDOWPOS_CENTERED,
                   fullscreenRequested ? winx : WindowWidth,
		   fullscreenRequested ? winy : WindowHeight,
                   flags)) == NULL)
    {
      UI_MessageBox(SDL_GetError(),
                    "Failed to create window",
                    MESSAGE_OK);
      die(0xe9a3);
    };
    SDL_Surface *sf = SDL_LoadBMP("lsb.bmp");
    if (sf) {
	SDL_SetWindowIcon(sdlWindow,sf);
	SDL_FreeSurface(sf);
    }
    SDL_VIDEOEXPOSE = SDL_RegisterEvents(1);
    if ((sdlRenderer = SDL_CreateRenderer(
                   sdlWindow,
                   -1,
                   0)) == NULL)
    {
      UI_MessageBox(SDL_GetError(),
                    "Failed to create Renderer",
                    MESSAGE_OK);
      die(0x519b);
    };
    // I disable this because of imgui, the game picture doesn't take the whole window anymore, so it's better to convert the coordinates ourselves
    // SDL_RenderSetLogicalSize(sdlRenderer,320,200);
    if ((sdlTexture = SDL_CreateTexture(
                   sdlRenderer,
                   SDL_PIXELFORMAT_RGB565,
                   SDL_TEXTUREACCESS_STREAMING,
                   320,200)) == NULL)
    {
      UI_MessageBox(SDL_GetError(),
                    "Failed to create texture",
                    MESSAGE_OK);
      die(0xff53);
    };
#else
    xxxError
#endif
	if (fullscreenRequested) {
	    fullscreenActive = true;
	    fullscreenRequested = false;
	    SDL_DisplayMode mode;
	    SDL_GetDesktopDisplayMode(0,&mode);
	    SDL_SetWindowDisplayMode(sdlWindow,&mode);
	}
#ifdef _DYN_WINSIZE
    if(!Hermes_Init()) g_error("No hermes...");
    from_palette = Hermes_PaletteInstance();
    to_palette = Hermes_PaletteInstance();
    converter = Hermes_ConverterInstance( HERMES_CONVERT_NORMAL );
    from_format = Hermes_FormatNew(8,0,0,0,0,TRUE);
#else
#ifdef SDL12
    SCRAP = SDL_CreateRGBSurface(SDL_SWSURFACE,screenWidth,screenHeight,8,0,0,0,0);
    {
      FILE *f;
      f = fopen("/run/shm/debug", "a");
      fprintf(f, "Creating Surface %d %d\n", screenWidth, screenHeight);
      fclose(f);
    };
    if (SCRAP == NULL)
    {
      g_error("Unable to get SDL surface: %s",SDL_GetError());
    };
#endif
#endif
  };

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForSDLRenderer(sdlWindow, sdlRenderer);
  ImGui_ImplSDLRenderer2_Init(sdlRenderer);

  // Load Fonts
  char font[FILENAME_MAX+29];
  snprintf(font,FILENAME_MAX+15,"%s/fonts/Vera.ttf",pwd);
  add_font(font);
  snprintf(font,FILENAME_MAX+28,"%s/fonts/diogenes.ttf",pwd);
  wall_font = add_font(font);
  snprintf(font,FILENAME_MAX+28,"%s/fonts/OldeEnglish.ttf",pwd);
  scroll_font = add_font(font);

  SDL_ShowCursor(SDL_ENABLE);
  cursorIsShowing = true;
  AutoMapWindow_RestoreVisibility();
  if(fullscreenRequested)
  {
    fullscreenActive = true;
#if defined SDL12
    //SDL_WM_ToggleFullScreen(WND);
#elif defined SDL20
    //NotImplemented(0x84ea);
#else
    xxxError
#endif
  }
  UI_Initialize_sounds();


  /*
   * Initialize the display in a 640x480 8-bit palettized mode,
   * requesting a software surface, or anything else....
   */

  /* Do the window-cha-cha. */
  speedTable[SPEED_GLACIAL].vblPerTick = 1000;
  speedTable[SPEED_MOLASSES].vblPerTick = 55;
  speedTable[SPEED_VERYSLOW].vblPerTick = 33;
  speedTable[SPEED_SLOW].vblPerTick = 22;
  speedTable[SPEED_NORMAL].vblPerTick = 15;
  speedTable[SPEED_FAST].vblPerTick = 11;
  speedTable[SPEED_QUICK].vblPerTick = 7;
  speedTable[SPEED_FTL].vblPerTick = 5;

  volumeTable[VOLUME_FULL].divisor = 1;
  volumeTable[VOLUME_HALF].divisor = 2;
  volumeTable[VOLUME_QUARTER].divisor = 4;
  volumeTable[VOLUME_EIGHTH].divisor = 8;
  volumeTable[VOLUME_OFF].divisor = 65536;

  MTRACE("*** First initialization ***\n");
  csbMessage.type=UIM_INITIALIZE;
  if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
  {
    PostQuitMessage(0x25);
  };

  /* Setup a 50ms timer. */
  sem = SDL_CreateSemaphore(1);
  timer = SDL_AddTimer(TImER?TImER:10, __Timer_Callback, (void*)(IDC_Timer));

  //SDL_WM_GrabInput(SDL_GRAB_ON);

  /********************************************
   *
   *       Main Execution Loop
   *
   ********************************************
   */
  if (get_nbkeys() == 0)
      UI_MessageBox("No keyboard shortcut !\nCheck that the csb.cfg file is available.","Warning",MESSAGE_OK);

  while (true)
  {

#ifdef USE_OLD_GTK
    gtk_main_iteration_do(FALSE); // This solution will always cost CPU instead of gtk_main()
#endif //USE_OLD_GTK
    // evert = app.WaitEvent();
    if (SDL_PollEvent(&evert) == 0)
    {
	SDL_SemWait(sem);
	timerInQueue=false;
	SDL_SemPost(sem);
      if(SDL_WaitEvent(&evert) == 0) {
	  printf("SDL_WaitEvent error %s\n",SDL_GetError());
	  continue;
      }
    }

    if (AutoMapWindow_HandlePointerEvent(evert)) {
	AutoMapWindow_PaintIfDirty();
	continue;
    }
    if ((evert.type == SDL_KEYDOWN || evert.type == SDL_TEXTINPUT)
        && AutoMapWindow_HandleKeyEvent(evert)) {
	AutoMapWindow_PaintIfDirty();
	continue;
    }
    ImGui_ImplSDL2_ProcessEvent(&evert);
    if (imgui_active) {
	switch(evert.type) {
	case SDL_MOUSEBUTTONUP:
	case SDL_KEYUP:
	case SDL_MOUSEMOTION: continue;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_KEYDOWN: if (!capturing_key) continue;
	}
    }
    // uint32_t tick = SDL_GetTicks();
    // printf("event type %x ticks %d\n",evert.type,tick);
    if (sdlQuitPending)
    {
//      FILE *f;
//      f = fopen("debug","a");
//      fprintf(f, "MainLook sdlQuitPending type= %d\n", evert.type);
//      fclose(f);
      if (SDL_QuitRequested())
      {
        if (evert.type != SDL_QUIT) continue;
      }
      else
      {
        sdlQuitPending = false;
      };
    };
    /* Listen for 'quick buttons' here. */
    /* Hail to the Great Message Struct! */
    MTRACE("msg=");
    switch( evert.type )
    {
      case SDL_QUIT:
        MTRACE("SDL_QUIT\n");
        cbAppDestroy();
        break;
      default:
        MTRACE("was ignored\n");
        break;
      case SDL_MOUSEMOTION:     Process_SDL_MOUSEMOTION(
                                         cursorIsShowing);
                                break;
      case SDL_USEREVENT:       Process_SDL_USEREVENT();       break;// __timer_loopback
      case SDL_MOUSEBUTTONDOWN: Process_SDL_MOUSEBUTTONDOWN(); break;
      case SDL_MOUSEBUTTONUP:   Process_SDL_MOUSEBUTTONUP();   break;
      case SDL_KEYDOWN:         Process_SDL_KEYDOWN();         break;
      // Notice that TEXTINPUT event are passed too, but since the keycodes accepted by the game are very limited, it's useless for now...
      case SDL_KEYUP:           Process_SDL_KEYUP();           break;
      case SDL_WINDOWEVENT:     Process_SDL_WINDOWEVENT(evert);    break;
      /*
      case SDL_ACTIVEEVENT:
        {
          if ( evert.active.state & SDL_APPACTIVE )
          {
            if ( evert.active.gain )
            {
              printfalll("App activated\n");
            }
            else
            {
              printf("App iconified\n");
            };
          };
        };
      */

    }; /* Eof Great Event Switch */
    if (imgui_active) {
	if (drawn) {
	    drawn = 0;
	} else {
	    post_render();
	    drawn = 0;
	}
/*    } else {
	for(int tb=0;tb<MSG_LIST_SIZE;tb++){
	    if (MsgList[tb].messagetime>0)
		MsgList[tb].messagetime--;
	} */
    }
    AutoMapWindow_PaintIfDirty();

  } /* Eof Lord Message Loop */

  Cleanup(true);
  done_imgui();
  doneCaches();
  printf("Quiting SDL.\n");

  /* Shutdown all subsystems */
  SDL_Quit();
  printf("Quiting....\n");
  return (0);
}

extern void ItemsRemaining(i32 mode); // CSBUI.cpp
extern const char *listing_title; // LinCSBUI.cpp
extern LISTING *listing; // AsciiDump.cpp
extern bool resetstartcsb;
bool show_listing,show_keys;
extern i32 lastTime;
extern bool skipLogo;
static bool show_coords,open_char_info;
extern i32 numState;
i32 launcher_power;

static void reset_game() {
    numState = 0; // reset the "stackState" from system.cpp
		  // because we are creating a new one here.
    verticalIntEnabled = false;
    lastTime = 0; // reset _MouseHandleEvents
    open_char_info = false; // Prevents a freeze when reseting a game with this open
    SDL_ShowCursor(SDL_ENABLE);
    cursorIsShowing = true;
    Cleanup(false);
    resetWhatToDo=resetgamesetup=resetstartcsb=resetprisondoor=resettag1=resettag2=true;
    csbMessage.type=UIM_INITIALIZE;
    if (CSBUI(&csbMessage) != UI_STATUS_NORMAL)
    {
	PostQuitMessage(0);
    }
    gameFrozen = false;
}

static bool fb_shown,fb2_shown,fb3_shown,fb4_shown;
extern bool chaosDisplayed,skipToDungeon,skipToResumeGame; // CSBCode.cpp

static int get_rank(int xp) {
    if (xp < 500) return 0;
    int rank = 1, base = 500;
    while (xp > base*2) {
	rank++;
	base*=2;
    }
    if (rank > 15) rank = 15;
    return rank;
}

static float get_progress(int xp) {
    int base = 500;
    while (xp > base*2) {
	base*=2;
    }
    int next;
    if (xp > base)
	xp -= base;
    next = base;
    float progress = 1.0-(next-xp)*1.0/next;
    return progress;
}

static void handle_skill(int n,int xp) {
    const char *ranks[] = { "", "Neophyte", "Novice", "Apprentice", "Journeyman", "Craftsman", "Artisan", "Adept", "Expert", "Master 1", "Master 2", "Mastger 3",
	"Master 4", "Master 5", "Master 6", "Archmaster" };
    const char *skills[] = { "Fighter", "Ninja", "Priest", "Wizard",
	"Swing (hidden fighter skill)", "Thrust (hidden fighter skill)", "Club (hidden fighter skill)", "Parry (hidden fighter skill)",
	"Steal (hidden ninja skill)", "Fight (hidden ninja skill)", "Throw (hidden ninja skill)", "Shoot (hidden ninja skill)",
	"Identify (hidden priest skill)", "Heal (hidden priest skill)", "Influence (hidden priest skill)", "Defend (Hidden priest skill)",
	"Fire (hidden wizard skill)", "Air (hidden wizzard skill)", "Earth (hidden wizard skill)", "Water (hidden wizard skill)" };
    char buf[80];
    snprintf(buf,80,"%s: %d %s",skills[n],xp,ranks[get_rank(xp)]);
    float progress = get_progress(xp);
    ImGui::TableNextColumn();
    ImGui::Text(buf);
    ImGui::TableNextColumn();
    // printf("green %g\n",255-progress*255);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram,ImVec4(1.0,1.0-progress,0,0.5));
    ImGui::ProgressBar(progress,ImVec2(100.0f, 0.0f));
    ImGui::PopStyleColor(1);
}

static int get_def(int chIdx,int mask = 8) {
    // Extracted from DamageCharacter with mask=4 (and there was a P4 parameter at 4 too).
    // Apparently mask is the slot chosen for the hit as a power of 2, 4 means slot 2 which is the head
    // and so 8 would be slot 3 which is the chest. The 2 most chosen slots are 2 and 3, corresponding to a mask value of 4 and 8 respectively.
    // Slots values found:
    // 0: shield hand
    // 1: sword hand
    // 2: head
    // 3: chest
    // 4: legs
    // 5: feet
    dReg D4,D5,D6,D0,D1;
    D5W = 0;
    for (D4W=D5W=D6W=0; D6W<=5; D6W++)
    {
      if ((mask & (1<<D6W)) == 0) continue;
      D4W++;
      D0W = D6W;
      D1UW = 0x8000;
      D5W = sw(D5W + TAG01680a(chIdx, D0W | D1W));
    }
    D5W += d.hero[chIdx].shieldStrength;
    return D5W;
}

extern bool skipToSavenPlay,skipReady;
extern bool saveRequestedFromMenu;
// The only test so far directly on nostaliga_mode : recharging ability of vi altars in Mouse.cpp
bool monsters_vulnerable_on_attack = true,nostalgia_mode = false,filter_dsa_door_sound,experimental_overlay;
extern char *PlayfileName; // csbui.cpp

int want_popup;
char popup_msg[80];
int yes_selected;

void open_popup(const char *msg) {
    imgui_active = true;
    want_popup = 1;
    strncpy(popup_msg,msg,80);
    popup_msg[79] = 0;
    yes_selected = 0;
}

static float active_red = 0.75, inc_active = 0.02;

static void stop_capture(int button) {
    captured_key = 0;
    capturing_key = false;
    int n = 0;
    while (mykeys[n].desc) {
	if (mykeys[n].active1) {
	    mykeys[n].active1 = false;
	    if (button == SDL_BUTTON_RIGHT) {
		mykeys[n].key1 = 0;
		keyxlate.setkey(0, mykeys[n].key, TYPESCAN, 0);
	    }
	    return;
	}
	if (mykeys[n].active2) {
	    mykeys[n].active2 = false;
	    if (button == SDL_BUTTON_RIGHT) {
		mykeys[n].key2 = 0;
		keyxlate.setkey(0, mykeys[n].key, TYPESCAN, 1);
	    }
	    return;
	}
	n++;
    }
    n = 0;
    while (mkeys[n].desc) {
	if (mkeys[n].active1) {
	    mkeys[n].active1 = false;
	    if (button == SDL_BUTTON_RIGHT) {
		mkeys[n].key1 = 0;
		keyxlate.setkey(0, mykeys[n].key, TYPEMSCANL, 0);
	    }
	    return;
	}
	n++;
    }
}

static void prepare_red(float &red) {
    red = active_red;
    active_red += inc_active;
    if (active_red > 1) {
	inc_active = -0.02;
	active_red += inc_active;
    } else if (active_red < 0.5) {
	inc_active = 0.02;
	active_red += inc_active;
    }
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(red, 0.26f, 0.40f, 1.0f) );
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(red, 0.26f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(red, 0.26f, 0.40f, 1.0f));
}

static void select_dungeon(char *dungeonName) {
    char path[FILENAME_MAX];
    strcpy(path,dungeonName);
    char *s = strrchr(path,'/');
    if (s) *s = 0;
    printf("path %s\n",path);
    chdir(path);
}

static void read_conf() {
    csb_push_config_state();
    char path[FILENAME_MAX+8];
    snprintf(path,FILENAME_MAX+8,"%s/csb.cfg",pwd);
    csb_set_config_file(path);
    TImER = csb_get_config_int("settings","timer",10);
    fullscreenRequested = csb_get_config_int("settings","fullscreen",false);
    WindowWidth = csb_get_config_int("settings","width", 320*4);
    WindowHeight = csb_get_config_int("settings","height", 200*4);
    winx = csb_get_config_int("settings","winx",320*4);
    winy = csb_get_config_int("settings","winy",200*4);
    gameSpeed = (SPEEDS)csb_get_config_int("settings","gameSpeed",4); // 4 seems to be SPEED_NORMAL
    show_coords = csb_get_config_int("settings","show_coords",0);
    monsters_vulnerable_on_attack = csb_get_config_int("settings","monsters_vulnerable_on_attack",0);
    nostalgia_mode = csb_get_config_int("settings","nostalgia_mode",0);
    strcpy(dungeonName, csb_get_config_string("settings","dungeonName",(char*)"dungeon.dat")); // dungeonName points to dungeonname which is char[FILENAME_MAX]
    select_dungeon(dungeonName);
    strcpy(graphicName,csb_get_config_string("settings","graphicName",(char*)"graphics.dat"));
    launcher_power = csb_get_config_int("settings","launcher_power",1);
    filter_dsa_door_sound = csb_get_config_int("settings","filter_dsa_door_sound",0);
    experimental_overlay = csb_get_config_int("settings","experimental_overlay",0);
    strncpy(translation, csb_get_config_string("settings","translation",(char*)""),10);
    translation[9] = 0;

    st_X = 320.0 / WindowWidth;
    st_Y = 200.0 / (WindowHeight - 20);
    keyxlate.read_conf();
    AutoMap_ReadSettings();
    csb_pop_config_state();
}

static void save_conf() {
    csb_push_config_state();
    char path[FILENAME_MAX+8];
    snprintf(path,FILENAME_MAX+8,"%s/csb.cfg",pwd);
    csb_set_config_file(path);
    csb_set_config_int("settings","timer",TImER);
    csb_set_config_int("settings","fullscreen",fullscreenActive);
    csb_set_config_int("settings","width",WindowWidth);
    csb_set_config_int("settings","height",WindowHeight);
    csb_set_config_int("settings","winx",winx);
    csb_set_config_int("settings","winy",winy);
    csb_set_config_int("settings","gameSpeed",gameSpeed);
    csb_set_config_int("settings","show_coords",show_coords);
    csb_set_config_int("settings","monsters_vulnerable_on_attack",monsters_vulnerable_on_attack);
    csb_set_config_int("settings","nostalgia_mode",nostalgia_mode);
    csb_set_config_string("settings","dungeonName",dungeonName);
    csb_set_config_string("settings","graphicName",graphicName);
    csb_set_config_int("settings","launcher_power",launcher_power);
    csb_set_config_int("settings","filter_dsa_door_sound",filter_dsa_door_sound);
    csb_set_config_int("settings","experimental_overlay",experimental_overlay);
    csb_set_config_string("settings","translation",translation);
    keyxlate.write_conf();
    AutoMap_WriteSettings();
    csb_pop_config_state();
}

static int mbase;		// Which message is top of the list

// BlitScreen():
// Entry to screen blitting, takes care of pause mode, eagle mode

// print_ingame():
// Add Message to Ingame Message List, using a printf() style format string.
typedef struct {
    int x,y,color,toScreen;
    bool centered;
    const char *text;
} txy;

#define MAX_XY 40

static txy myxy[MAX_XY];
typedef struct {
    int x1,x2,y1,y2;
    const char *text;
    int updates;
} twall;

typedef struct {
    RectPos *r;
    char *text;
} tscroll;

static tscroll scroll;
static twall mywall;

void print_ingame_wall(int x1, int x2, int y1, int y2, const char *text) {
    mywall.x1 = x1;
    mywall.x2 = x2;
    mywall.y1 = y1;
    mywall.y2 = y2;
    mywall.text = text;
    mywall.updates = 0;
}

void viewport_update() {
    if (mywall.text) {
	mywall.updates++;
	if (mywall.updates > 1)
	    mywall.text = NULL;
    }
}

int nb_xy;
int dx,dy;

void print_ingame_xy(int x, int y, int color, const char *text,int tScreen, bool centered) {
    if (nb_xy == MAX_XY) {
	printf("xy overflow\n");
	exit(1);
    }
    for (int n=0; n<nb_xy; n++) {
	if (myxy[n].x == x && myxy[n].y == y) {
	    printf("overwriting message %d\n",n);
	    myxy[n].text = text;
	    myxy[n].toScreen = tScreen;
	    return;
	}
    }
    myxy[nb_xy].x = x;
    myxy[nb_xy].y = y;
    myxy[nb_xy].color = color;
    myxy[nb_xy].toScreen = tScreen;
    myxy[nb_xy].centered = centered;
    myxy[nb_xy++].text = text;
}

void print_ingame(const char *format, ...)
{
   va_list ap;
   va_start(ap,format);
   vsprintf(MsgList[mbase].message,format,ap);
   va_end(ap);

   MsgList[mbase].color = -1;

   mbase++;
   if(mbase>=MSG_LIST_SIZE) mbase=0;
}

static bool autoscroll = false;

void print_ingame_color(int color, const char *format, ...) {
   va_list ap;
   va_start(ap,format);
   autoscroll = true;
   int prec = mbase-1;
   if (prec < 0) prec = MSG_LIST_SIZE-1;
   char *s = MsgList[prec].message;
   if (*format != 10 && *s && s[strlen(s)-1] != 10) { // append to previous string
       vsprintf(&MsgList[prec].message[strlen(MsgList[prec].message)],format,ap);
       va_end(ap);
       return;
   } else
       vsprintf(MsgList[mbase].message,format,ap);
   va_end(ap);

   MsgList[mbase].color = color;

   mbase++;
   if(mbase>=MSG_LIST_SIZE) mbase=0;
}
// clear_ingame_message_list():
// Clear the ingame message list

void clear_ingame_message_list(void)
{
   int ta;

   mbase = 0;
   for(ta = 0; ta < MSG_LIST_SIZE; ta ++){
      sprintf(MsgList[ta].message," ");
   }
}

void viewport_xy(int x, int y) {
    dx = x;
    dy = y;
    printf("viewport %d,%d\n",x,y);
}

void clear_xy(RectPos *r) {
    if (r) {
	for (int n=0; n<nb_xy; n++) {
	    if (myxy[n].x >= r->w.x1 && myxy[n].x <= r->w.x2 &&
		    myxy[n].y >= r->w.y1 && myxy[n].y <= r->w.y2) {
		if (nb_xy > n+1) {
		    memmove(&myxy[n],&myxy[n+1],sizeof(txy)*(nb_xy-(n+1)));
		}
		nb_xy--;
		n--;
		continue;
	    }
	}
	return;
    }
    nb_xy = 0;
    viewport_xy(0,0);
}

void print_ingame_scroll(RectPos *r,char *text) {
    scroll.r = r;
    scroll.text = text;
}

void end_scroll() {
    scroll.text = NULL;
}

static void render_overlay_interface() {
    // Renders a text overlay interface on top of the imgui screen
    // Not certain it's the fastest way to do this thing, but imgui is really efficient, so let's try that...
#if 0
    bool active = false;
    for(int tb=0;tb<MSG_LIST_SIZE;tb++){
	if (MsgList[tb].messagetime>0) {
	    active = true;
	    break;
	}
    }
    if (!active) return; // We try to avoid to create the fullscreen window if useless... !
#endif
    if (!nb_xy && (chaosDisplayed || !d.NumGraphic || !d.PrimaryButtonList)) return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    int size = (WindowHeight-DY)/25*4;
    ImGui::SetNextWindowPos({viewport->WorkPos.x, (float)(WindowHeight-size+2)});
    ImGui::SetNextWindowSize({viewport->WorkSize.x,(float)size-2});
    size = (WindowHeight-DY)/25;

    if (ImGui::Begin("Fullscreen window", NULL, flags)) {
	if (ImGui::IsAnyItemHovered()) {
	    SDL_ShowCursor(SDL_ENABLE);
	    imgui_active = true;
	}
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,255,255,255));
       for(int tb=0;tb<MSG_LIST_SIZE;tb++){

	   int ta=tb+mbase;

	   if(ta>=MSG_LIST_SIZE)
	       ta-=MSG_LIST_SIZE;

	   if(*MsgList[ta].message && *MsgList[ta].message!=32){

	       // MsgList[ta].messagetime -= 1;

	       // ImGui::SetCursorPos(ImVec2(0,WindowHeight-DY-size-size*(((MSG_LIST_SIZE-1)-tb))));
	       ImGui::PushFont(NULL,size-6); // Not sure about the font-6, it works for font size 14 -> height of 20, but I doubt it will work all the time
	       // printf("render %d,%s\n",MsgList[ta].color,MsgList[ta].message);
	       ImGui::PushTextWrapPos();
	       char *s = MsgList[ta].message;
	       char old = s[strlen(s)-1];
	       if (old == 10) s[strlen(s)-1] = 0;
	       switch(MsgList[ta].color) {
	       case 1: ImGui::TextColored(ImVec4(0.43f, 0.43f, 0.43f, 1.0f), s); break;
	       case 2: ImGui::TextColored(ImVec4(0.57f, 0.57f, 0.57f, 1.0f), s); break;
	       case 3: ImGui::TextColored(ImVec4(0.43f, 0.14f, 0.00f, 1.0f), s); break;
	       case 4: ImGui::TextColored(ImVec4(0.00f, 0.86f, 0.86f, 1.0f), s); break;
	       case 5: ImGui::TextColored(ImVec4(0.57f, 0.29f, 0.00f, 1.0f), s); break;
	       case 6: ImGui::TextColored(ImVec4(0.00f, 0.57f, 0.00f, 1.0f), s); break;
	       case 7: ImGui::TextColored(ImVec4(0.00f, 0.86f, 0.00f, 1.0f), s); break;
	       case 8: ImGui::TextColored(ImVec4(1.00f, 0.00f, 0.00f, 1.0f), s); break;
	       case 9: ImGui::TextColored(ImVec4(1.00f, 0.71f, 0.00f, 1.0f), s); break;
	       case 10: ImGui::TextColored(ImVec4(0.86f, 0.57f, 0.43f, 1.0f), s); break;
	       case 11: ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), s); break;
	       case 12: ImGui::TextColored(ImVec4(0.29f, 0.29f, 0.29f, 1.0f), s); break;
	       case 13: ImGui::TextColored(ImVec4(0.71f, 0.71f, 0.71f, 1.0f), s); break;
	       case 14: ImGui::TextColored(ImVec4(0.2f, 0.2f, 1.0f, 1.0f), s); break;
	       case 15: ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), s); break;
	       default:
		       // ImGui::Text(s);
		       // printf("overlay: %s\n",s);
		       ImGui::TextWrapped(s);
		       break;
	       }
	       s[strlen(s)-1] = old;
	       ImGui::PopTextWrapPos();
	       ImGui::PopFont();
	   }
       }
       ImGui::PopStyleColor();
       if (autoscroll && !ImGui::IsItemActive()) {
	   printf("autoscroll %g\n",ImGui::GetScrollMaxY());
	   ImGui::SetScrollHereY(1.0f);
	   autoscroll = false;
       }
       ImGui::End();
    }

    if (nb_xy || mywall.text) {
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;
	ImGui::SetNextWindowPos({viewport->WorkPos.x, viewport->WorkPos.y});
	ImGui::SetNextWindowSize({viewport->WorkSize.x,viewport->WorkSize.y});
	int dx0 = dx, dy0 = dy;
	if (dx0 < 10 && dy0 < 10) { // Well Word60 has the initial coordinates for this, and it's 0,3...
	    dx0 = 0;
	    dy0 = 33;
	    printf("dx0 %d,%d\n",dx0,dy0);
	} else {
	    printf("dx0 = viewport = %d,%d\n",dx0,dy0);
	}
	if (ImGui::Begin("Fullscreen xy window", NULL, flags)) {
	    float zx = WindowWidth / 320.0; // viewport->WorkSize.x/320.0;
	    float zy = (WindowHeight-DY)/ 200.0; // viewport->WorkSize.y/200.0;
	    ImGui::PushFont(NULL,size-6);
	    for (int n=0; n<nb_xy; n++) {
		// No idea why there's a vertical alignment problem, 15 is the best approximated fix so far
		ImVec2 size;
		if (myxy[n].centered) // on x only for now because that's what the game does
		    size = ImGui::CalcTextSize(myxy[n].text);
		if (myxy[n].toScreen)
		    ImGui::SetCursorPos(ImVec2((myxy[n].x)*zx - (myxy[n].centered ? size[0]/2 : 0),(myxy[n].y-5)*zy));
		else
		    ImGui::SetCursorPos(ImVec2((myxy[n].x+dx0)*zx - (myxy[n].centered ? size[0]/2 : 0),(myxy[n].y+dy0-5)*zy));
		switch(myxy[n].color) {
		case 0: ImGui::TextColored(ImVec4(0.0f, 0.0f, 0.0f, 1.0f), myxy[n].text); break;
		case 1: ImGui::TextColored(ImVec4(0.43f, 0.43f, 0.43f, 1.0f), myxy[n].text); break;
		case 2: ImGui::TextColored(ImVec4(0.57f, 0.57f, 0.57f, 1.0f), myxy[n].text); break;
		case 3: ImGui::TextColored(ImVec4(0.43f, 0.14f, 0.00f, 1.0f), myxy[n].text); break;
		case 4: ImGui::TextColored(ImVec4(0.00f, 0.86f, 0.86f, 1.0f), myxy[n].text); break;
		case 5: ImGui::TextColored(ImVec4(0.57f, 0.29f, 0.00f, 1.0f), myxy[n].text); break;
		case 6: ImGui::TextColored(ImVec4(0.00f, 0.57f, 0.00f, 1.0f), myxy[n].text); break;
		case 7: ImGui::TextColored(ImVec4(0.00f, 0.86f, 0.00f, 1.0f), myxy[n].text); break;
		case 8: ImGui::TextColored(ImVec4(1.00f, 0.00f, 0.00f, 1.0f), myxy[n].text); break;
		case 9: ImGui::TextColored(ImVec4(1.00f, 0.71f, 0.00f, 1.0f), myxy[n].text); break;
		case 10: ImGui::TextColored(ImVec4(0.86f, 0.57f, 0.43f, 1.0f), myxy[n].text); break;
		case 11: ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), myxy[n].text); break;
		case 12: ImGui::TextColored(ImVec4(0.29f, 0.29f, 0.29f, 1.0f), myxy[n].text); break;
		case 13: ImGui::TextColored(ImVec4(0.71f, 0.71f, 0.71f, 1.0f), myxy[n].text); break;
		case 14: ImGui::TextColored(ImVec4(0.2f, 0.2f, 1.0f, 1.0f), myxy[n].text); break;
		case 15: ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), myxy[n].text); break;
		default:
			 // ImGui::Text(s);
			 // printf("overlay: %s\n",s);
			 ImGui::TextWrapped(myxy[n].text);
			 break;
		}
	    }
	    ImGui::PopFont();

	    if (mywall.text) {
		size = 0x4a*zx/3.5;
		ImGui::PushFont(wall_font,size-6);
		// The wrap support in imgui is crap for now, you can't center horizontaly and verticaly a wrapped text, so here is a workaround...
		// ImGui::PushTextWrapPos((mywall.x2+127)*zx);
		char buf[100];
		strcpy(buf,mywall.text);
		SDL_strupr(buf);

		int startx = (mywall.x1+45)*zx;
		int starty = (mywall.y1+33)*zy-15;
		int maxwidth = (mywall.x2+122)*zx-startx;
		int maxheight = 70*zy;
		int line = 0;
		char *text[10];
		char *start = buf;
		ImVec2 wh;
		char *s;
		do {
		    wh = ImGui::CalcTextSize(start);
		    s = &start[strlen(start)-1];
		    while (wh[0] > maxwidth) {
			if (*s == 0) {
			    *s = ' ';
			    s--;
			}
			while (*s != ' ' && s > start)
			    s--;
			*s = 0; // assuming s > buf, which should be the case with any normal text
			wh = ImGui::CalcTextSize(start);
		    }
		    text[line++] = start;
		    if (*s == 0)
			start = s+1;
		} while (*s == 0);
		starty = starty+(maxheight-line*wh[1])/2;
		for (int n=0; n<line; n++) {
		    wh = ImGui::CalcTextSize(text[n]);
		    ImGui::SetCursorPos(ImVec2(startx+(maxwidth-wh[0])/2+1,starty+1));
		    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f),text[n]);
		    ImGui::SetCursorPos(ImVec2(startx+(maxwidth-wh[0])/2-1,starty-1));
		    ImGui::TextColored(ImVec4(0.43f, 0.43f, 0.43f, 1.0f),text[n]);
		    starty += wh[1];
		}
		ImGui::PopFont();
	    }

	    if (scroll.text) {
		// Scrolls work almost the same way as wall texts, so re-use the centering code just a few lines up...
		char buf[100];
		strcpy(buf,scroll.text);

		int startx = (scroll.r->w.x1+35)*zx;
		int starty = (scroll.r->w.y1+35)*zy+33;
		int maxwidth = (scroll.r->w.x2-20)*zx-startx;
		int maxheight = (scroll.r->w.y2+10)*zy+33-starty;
		ImGui::PushFont(scroll_font,maxheight/MAX_LINES_IN_SCROLL*2);
		int line = 0;
		char *text[10];
		char *start = buf;
		ImVec2 wh;
		char *s;
		do {
		    wh = ImGui::CalcTextSize(start);
		    s = &start[strlen(start)-1];
		    while (wh[0] > maxwidth) {
			if (*s == 0) {
			    *s = ' ';
			    s--;
			}
			while (*s != ' ' && s > start)
			    s--;
			*s = 0; // assuming s > buf, which should be the case with any normal text
			wh = ImGui::CalcTextSize(start);
		    }
		    text[line++] = start;
		    if (*s == 0)
			start = s+1;
		} while (*s == 0);
		starty = starty+(maxheight-line*wh[1])/2;
		for (int n=0; n<line; n++) {
		    wh = ImGui::CalcTextSize(text[n]);
		    ImGui::SetCursorPos(ImVec2(startx+(maxwidth-wh[0])/2,starty));
		    ImGui::TextColored(ImVec4(0.0f, 0.0f, 0.0f, 1.0f),text[n]);
		    starty += wh[1];
		}
		ImGui::PopFont();
	    }

	    ImGui::End();
	}
    }
}

imgui_addons::ImGuiFileBrowser file_dialog; // As a class member or globally

extern void ReadTranslationFile(char*);

void post_render() {
    static bool was_active;
    drawn = 1;
    SDL_Rect r;
    bool open = false,save = false,open_dungeon = false, open_playback = false;
    if (!d.NumGraphic) imgui_active = true;

    // Start the Dear ImGui frame
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (want_popup)
	ImGui::OpenPopup("Question");

    if (chaosDisplayed) ImGui::BeginDisabled();
    if (ImGui::BeginMainMenuBar())
    {
	// I don't know why you should use IsWindowHovered and not IsItemHovered for a mainmenubar, but that's the way it is...
	bool on_menubar = ImGui::IsWindowHovered();
	if (on_menubar) {
	    imgui_active = true;
	    was_active = true;
	}
	r.y = DY;
	r.x = 0;
	r.w = WindowWidth;
	r.h = WindowHeight - DY;
	if (ImGui::BeginMenu(_("File")))
	{
	    imgui_active = true;
	    was_active = true;
	    if (ImGui::MenuItem("Reset", NULL,false,d.partyLevel != 255 && d.NumGraphic > 0)) {
		skipLogo = true;
		reset_game();
		// _CALL0 (_4_,st_ReadEntireGame);
	    }
	    if (ImGui::MenuItem(_("Select dungeon.dat...")))
		open_dungeon = true;
	    if (ImGui::MenuItem(_("Load saved game"),NULL,false,d.NumGraphic>0))
		open = true;
	    if (ImGui::MenuItem(_("Save game"),NULL,false,d.partyLevel != 255 && d.NumGraphic>0
				&& d.NumCharacter > 0 && d.PotentialCharacterOrdinal == 0))
		save = true;
	    // Limited playback in front the dungeon door
	    if (ImGui::MenuItem(_("Playback.."), NULL,false,d.partyLevel == 255 && d.NumGraphic > 0))
		open_playback = true;
	    if (ImGui::MenuItem(_("Save params"), NULL))
		save_conf();
	    if (ImGui::MenuItem(_("Quit"), NULL))
		// Impossible to insert an UI_MessageBox here, post_render can't loop on itself
		cbAppDestroy();
	    ImGui::EndMenu();
	} else if (!fb_shown && !on_menubar && !fb2_shown && !fb3_shown && d.NumGraphic && !want_popup) {
	    imgui_active = false;
	}

	if (ImGui::BeginMenu(_("Speed"))) {
	    imgui_active = true;
	    was_active = true;
	    if (ImGui::MenuItem(_("Glacial"),NULL,gameSpeed == SPEED_GLACIAL))
		gameSpeed = SPEED_GLACIAL;
	    if (ImGui::MenuItem(_("Molasses"),NULL,gameSpeed == SPEED_MOLASSES))
		gameSpeed = SPEED_MOLASSES;
	    if (ImGui::MenuItem(_("Very Slow"),NULL,gameSpeed == SPEED_VERYSLOW))
		gameSpeed = SPEED_VERYSLOW;
	    if (ImGui::MenuItem(_("Slow"),NULL,gameSpeed == SPEED_SLOW))
		gameSpeed = SPEED_SLOW;
	    if (ImGui::MenuItem(_("Normal"),NULL,gameSpeed == SPEED_NORMAL))
		gameSpeed = SPEED_NORMAL;
	    if (ImGui::MenuItem(_("Fast"),NULL,gameSpeed == SPEED_FAST))
		gameSpeed = SPEED_FAST;
	    if (ImGui::MenuItem(_("Quick as a bunny"),NULL,gameSpeed == SPEED_QUICK))
		gameSpeed = SPEED_QUICK;
	    if (ImGui::MenuItem(_("Faster Than Light"),NULL,gameSpeed == SPEED_FTL))
		gameSpeed = SPEED_FTL;
	    ImGui::Separator();
	    ImGui::MenuItem(_("Extra Ticks"),NULL,&extraTicks);
	    ImGui::MenuItem(_("Player Clock"),NULL,&playerClock);
	    ImGui::EndMenu();
	}
	if (ImGui::BeginMenu(_("Misc")))
	{
	    imgui_active = true;
	    was_active = true;
	    if (ImGui::BeginMenu(_("Translation..."))) {
		if (ImGui::MenuItem(_("Original texts"), NULL, false)) { ReadTranslationFile((char*)""); clear_xy(); mywall.text = NULL; scroll.text = NULL; }
		if (ImGui::MenuItem(_("English"), NULL, false)) { ReadTranslationFile((char*)"en.po"); clear_xy(); mywall.text = NULL; scroll.text = NULL; }
		if (ImGui::MenuItem(_("French"), NULL, false)) { ReadTranslationFile((char*)"fr.po"); clear_xy(); mywall.text = NULL; scroll.text = NULL; }
		ImGui::EndMenu();
	    }
	    if (ImGui::MenuItem(_("Keyboard shortcuts"), NULL, false)) {
		int n = 0;
		while (mykeys[n].desc) {
		    mykeys[n].key1 = mykeys[n].key2 = 0;
		    i32 *key = keyxlate.getkey(mykeys[n].key,0);
		    if (key) mykeys[n].key1 = key[0];
		    key = keyxlate.getkey(mykeys[n].key,1);
		    if (key) mykeys[n].key2 = key[0];
		    n++;
		}
		n = 0;
		while (mkeys[n].desc) {
		    mkeys[n].key1 = mkeys[n].key2 = 0;
		    i32 *key = keyxlate.getkey(mkeys[n].key,0);
		    if (key) mkeys[n].key1 = key[0];
		    n++;
		}
		show_keys = true;
	    }
	    ImGui::MenuItem(_("Party coordinates"), NULL,&show_coords);
	    if (ImGui::BeginMenu(_("Map"))) {
		if (ImGui::MenuItem(_("Window"), NULL, AutoMapWindow_IsShown())) {
		    if (AutoMapWindow_IsShown()) AutoMapWindow_Hide();
		    else AutoMapWindow_Show();
		}
		if (ImGui::BeginMenu(_("Show level"))) {
		    if (ImGui::MenuItem(_("Follow party"), NULL, AutoMap_FollowsParty()))
			AutoMap_FollowParty();
		    ImGui::Separator();
		    int levelCount = AutoMap_LevelCount();
		    int shownLevel = AutoMap_ShownLevel();
		    for (int level = 0; level < levelCount; level++) {
			char label[32];
			snprintf(label, sizeof(label), "Level %d", level);
			bool selected = !AutoMap_FollowsParty() && shownLevel == level;
			if (ImGui::MenuItem(label, NULL, selected))
			    AutoMap_ShowLevel(level);
		    }
		    if (levelCount == 0) {
			char none[] = "Level 0";
			ImGui::MenuItem(none, NULL, false, false);
		    }
		    ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::BeginMenu(_("Vision"))) {
		    int vision = AutoMap_Vision();
		    if (ImGui::MenuItem(_("Fog of war"), NULL, vision == 0))
			AutoMap_SetVision(0);
		    if (ImGui::MenuItem(_("Explored, live"), NULL, vision == 1))
			AutoMap_SetVision(1);
		    if (ImGui::MenuItem(_("Whole level"), NULL, vision == 2))
			AutoMap_SetVision(2);
		    ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(_("Spoilers"))) {
		    int spoilerLevel = AutoMap_SpoilerLevel();
		    if (ImGui::MenuItem(_("Seen"), NULL, spoilerLevel == 0))
			AutoMap_SetSpoilerLevel(0);
		    if (ImGui::MenuItem(_("Learned"), NULL, spoilerLevel == 1))
			AutoMap_SetSpoilerLevel(1);
		    if (ImGui::MenuItem(_("All"), NULL, spoilerLevel == 2))
			AutoMap_SetSpoilerLevel(2);
		    ImGui::EndMenu();
		}
		if (ImGui::MenuItem(_("Highlight explored"), NULL, AutoMap_ExploredIsHighlighted(), AutoMap_Vision() == 2))
		    AutoMap_SetExploredHighlight(!AutoMap_ExploredIsHighlighted());
		if (ImGui::MenuItem(_("Small wall decorations"), NULL, AutoMapWindow_SmallWallDecorations()))
		    AutoMapWindow_SetSmallWallDecorations(!AutoMapWindow_SmallWallDecorations());
		if (ImGui::BeginMenu(_("Show"))) {
		    struct { AutoMapShown what; const char *label; bool separatorAfter; } rows[] = {
			{AutoMapShowWeapons, _("Weapons"), false},
			{AutoMapShowArmour, _("Armour"), false},
			{AutoMapShowPotions, _("Potions"), false},
			{AutoMapShowScrolls, _("Scrolls"), false},
			{AutoMapShowChests, _("Chests"), false},
			{AutoMapShowFood, _("Food"), false},
			{AutoMapShowKeys, _("Keys"), false},
			{AutoMapShowOtherItems, _("Other items"), true},
			{AutoMapShowMonsters, _("Monsters"), true},
			{AutoMapShowWallDecorations, _("Wall decorations"), false},
			{AutoMapShowWallWriting, _("Wall writing"), false},
			{AutoMapShowFloorDecorations, _("Floor decorations"), false},
		    };
		    size_t rowCount = sizeof(rows) / sizeof(rows[0]);
		    if (ImGui::MenuItem(_("Show all")))
			for (size_t i = 0; i < rowCount; i++) AutoMapWindow_SetShows(rows[i].what, true);
		    if (ImGui::MenuItem(_("Hide all")))
			for (size_t i = 0; i < rowCount; i++) AutoMapWindow_SetShows(rows[i].what, false);
		    ImGui::Separator();
		    for (size_t i = 0; i < rowCount; i++) {
			bool shown = AutoMapWindow_Shows(rows[i].what);
			if (ImGui::MenuItem(rows[i].label, NULL, shown))
			    AutoMapWindow_SetShows(rows[i].what, !shown);
			if (rows[i].separatorAfter) ImGui::Separator();
		    }
		    ImGui::EndMenu();
		}
		ImGui::EndMenu();
	    }
	    bool enabled = (ItemsRemainingOK
		    && (encipheredDataFile==NULL)
		    && !simpleEncipher);
	    if (ImGui::MenuItem(_("Non-CSB Items"), NULL,false,enabled)) ItemsRemaining(1);
	    ImGui::MenuItem(_("DM Rules"), NULL,&DM_rules);
	    if (ImGui::MenuItem(_("Truetype text replacement"),NULL, &experimental_overlay)) {
		clear_xy();
		scroll.text = NULL;
		mywall.text = NULL;
		ReadTranslationFile(translation);
	    }

	    ImGui::MenuItem(_("Filter DSA door sounds (for Conflux/DMCoE!)"),NULL, &filter_dsa_door_sound);
	    if (ImGui::MenuItem(_("Extended character info..."),NULL,false,d.NumCharacter > 0))
		open_char_info = true;
	    if (ImGui::BeginMenu(_("Launchers power"))) {
		if (ImGui::MenuItem(_("1 (default)"), NULL, launcher_power == 1)) launcher_power = 1;
		if (ImGui::MenuItem("2", NULL, launcher_power == 2)) { launcher_power = 2; nostalgia_mode = false; }
		if (ImGui::MenuItem("3", NULL, launcher_power == 3)) { launcher_power = 3; nostalgia_mode = false; }
		if (ImGui::MenuItem("4", NULL, launcher_power == 4)) { launcher_power = 4; nostalgia_mode = false; }
		if (ImGui::MenuItem("5", NULL, launcher_power == 5)) { launcher_power = 5; nostalgia_mode = false; }
		if (ImGui::MenuItem("6", NULL, launcher_power == 6)) { launcher_power = 6; nostalgia_mode = false; }
		ImGui::EndMenu();
	    }
	    if (ImGui::MenuItem(_("Monsters vulnerable on attack"),NULL,&monsters_vulnerable_on_attack))
		if (monsters_vulnerable_on_attack)
		    nostalgia_mode = false;
	    if (ImGui::MenuItem(_("Nostalgia Mode"), NULL, &nostalgia_mode)) {
		monsters_vulnerable_on_attack = false;
		launcher_power = 1;
	    }
	    ImGui::EndMenu();
	} else if (!imgui_active) {
	    if (was_active && !cursorIsShowing) {
		hide_system_pointer();
		// Tried RemoveCursor / ShowCursor, problem is last call to ShowCursor leaves a permanent cursor on game screen
		// so it's better to fight a little with mouse cursors when opening the mneu for now
		// until I find something better
		was_active = false;
	    }
	}

	if (show_coords && d.pLevelDescriptors) {
	    // Apparently d.pLevelDescriptors becomes unavailable during the loading of a saved game...
	    ImGui::SameLine(ImGui::GetWindowWidth() - 100);
	    ImGui::Text("Party %d,%d,%d",d.partyLevel,d.partyX+d.pLevelDescriptors[d.partyLevel].offsetX,d.partyY+d.pLevelDescriptors[d.partyLevel].offsetY);
	}
	ImGui::EndMainMenuBar();
    }
    if (chaosDisplayed) ImGui::EndDisabled();

    //Remember the name to ImGui::OpenPopup() and showFileDialog() must be same...
    if(open) {
	fb_shown = true;
	ImGui::OpenPopup(_("Load saved game"));
    } else if (open_dungeon) {
	fb3_shown = true;
	ImGui::OpenPopup(_("Select dungeon.dat..."));
    } else if (open_playback) {
	fb_shown = true;
	ImGui::OpenPopup(_("Select playback file..."));
    } else if (save) {
	fb2_shown = true;
        ImGui::OpenPopup(_("Save game"));
    }

    /* Optional third parameter. Support opening only compressed rar/zip files.
     * Opening any other file will show error, return false and won't close the dialog.
     */
    if(file_dialog.showFileDialog(_("Select playback file..."), imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(700, 310), "*.log")) {
	PlayfileName = (char*)file_dialog.selected_path.c_str();
	Process_ecode_IDC_Playback();
	fb_shown = false;
    } else if (fb_shown && !ImGui::IsPopupOpen(_("Select playback file...")) && !ImGui::IsPopupOpen(_("Load saved game")))
	fb_shown = false;

    if(file_dialog.showFileDialog(_("Load saved game"), imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(700, 310), "csb*"))
    {
	opened_file = (char*)file_dialog.selected_path.c_str();
	clear_xy();
	skipToDungeon = true;
	skipToResumeGame = true;
	skipReady = true;
	skipLogo = true;
	reset_game();
	fb_shown = false;
    } else if (fb_shown && !ImGui::IsPopupOpen(_("Load saved game")) && !ImGui::IsPopupOpen(_("Select playback file..."))) {
	// cancel was pressed!
	fb_shown = false;
    }

    if(file_dialog.showFileDialog(_("Select dungeon.dat..."), imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(700, 310), "dungeon.dat,*.dat"))
    {
	// strdup because this string is freed the next time the file selector is used, and if it's to load a save game, it's too bad!
	strcpy(dungeonName,file_dialog.selected_path.c_str());
	select_dungeon(dungeonName);
	strcpy(graphicName,"graphics.dat");
	FILE *f = fopen(graphicName,"rb");
	clear_xy();
	if (!f) {
	    fb4_shown = true;
	    ImGui::OpenPopup(_("Select graphics.dat..."));
	} else
	    fclose(f);
	if (!fb4_shown) {
	    root = (char*)"";
	    skipToDungeon = false;
	    reset_game();
	}
	fb3_shown = false;
    } else if (fb3_shown && !ImGui::IsPopupOpen(_("Select dungeon.dat...")))
	fb3_shown = false;

    if(file_dialog.showFileDialog(_("Select graphics.dat..."), imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(700, 310), "graphics.dat,*.dat")) {
	strcpy(graphicName,file_dialog.selected_path.c_str());
	fb4_shown = false;
	// chain on to the selected dungeon.dat
	root = (char*)"";
	skipToDungeon = false;
	reset_game();
    } else if (fb4_shown && !ImGui::IsPopupOpen(_("Select graphics.dat...")))
	cbAppDestroy();

    if(file_dialog.showFileDialog(_("Save game"), imgui_addons::ImGuiFileBrowser::DialogMode::SAVE, ImVec2(700, 310), "csb*"))
    {
	opened_file = (char*)file_dialog.selected_path.c_str();
	fb2_shown = false;
	skipToSavenPlay = true;
	// Calling DispatchCSB(st_DisplayDiskMenu) here pushed the disk menu on top of
	// whatever the game was suspended in and crashed when it returned. Ask instead,
	// like the disk button; the game opens the menu at its next safe point.
	saveRequestedFromMenu = true;
    } else if (fb2_shown && !ImGui::IsPopupOpen(_("Save game"))) {
	// cancel was pressed!
	fb2_shown = false;
    }
    if (show_keys) {
	if (ImGui::Begin("Keys",&show_keys)) {
	    bool popstyle = false;
	    imgui_active = true;
	    static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
	    ImGui::Text("Left click to cancel key, Right click to delete input");
	    if (ImGui::BeginTable("table1", 3, flags))
	    {
		// Display headers so we can inspect their interaction with borders
		// (Headers are not the main purpose of this section of the demo, so we are not elaborating on them now. See other sections for details)
		ImGui::TableSetupColumn(" ");
		ImGui::TableSetupColumn("Key 1");
		ImGui::TableSetupColumn("Key 2");
		ImGui::TableHeadersRow();

		for (int row = 0; mykeys[row].desc; row++)
		{
		    ImGui::TableNextRow();
		    ImGui::TableSetColumnIndex(0);
		    ImGui::TextUnformatted(mykeys[row].desc);
		    ImGui::TableSetColumnIndex(1);

		    float red = 0.13f;
		    // The handling of each button is quite long to my dismay, it's because ImGui doesn't seem very convenient to generate color buttons, you must use styles, and
		    // 3 styles for each button, it's heavy and not convenient. It would be much shorter without the color change, but it would be less fun and less nice!
		    ImGui::PushID(row*2);
		    if (mykeys[row].active1) {
			prepare_red(red);
			popstyle = true;
		    }

		    if (ImGui::Button(SDL_GetScancodeName((SDL_Scancode)mykeys[row].key1), ImVec2(-FLT_MIN, 0.0f))) {
			mykeys[row].active1 = true;
			capturing_key = true;
			captured_key = 0;
		    }
		    if (popstyle) {
			ImGui::PopStyleColor(3);
			popstyle = false;
		    }
		    ImGui::PopID();
		    if (mykeys[row].active1 && captured_key) {
			mykeys[row].active1 = false;
			capturing_key = false;
			mykeys[row].key1 = captured_key;
			keyxlate.setkey(captured_key, mykeys[row].key, TYPESCAN, 0);
			captured_key = 0;
		    }

		    ImGui::TableSetColumnIndex(2);
		    ImGui::PushID(row*2+1);
		    if (mykeys[row].active2) {
			prepare_red(red);
			popstyle = true;
		    }

		    if (ImGui::Button(SDL_GetScancodeName((SDL_Scancode)mykeys[row].key2), ImVec2(-FLT_MIN, 0.0f))) {
			mykeys[row].active2 = true;
			capturing_key = true;
			captured_key = 0;
		    }
		    if (popstyle) {
			ImGui::PopStyleColor(3);
			popstyle = false;
		    }
		    ImGui::PopID();
		    if (mykeys[row].active2 && captured_key) {
			mykeys[row].active2 = false;
			capturing_key = false;
			mykeys[row].key2 = captured_key;
			keyxlate.setkey(captured_key, mykeys[row].key, TYPESCAN, 1);
			captured_key = 0;
		    }
		}
		for (int row = 0; mkeys[row].desc; row++)
		{
		    ImGui::TableNextRow();
		    ImGui::TableSetColumnIndex(0);
		    ImGui::TextUnformatted(mkeys[row].desc);
		    ImGui::TableSetColumnIndex(1);

		    float red = 0.13f;
		    // The handling of each button is quite long to my dismay, it's because ImGui doesn't seem very convenient to generate color buttons, you must use styles, and
		    // 3 styles for each button, it's heavy and not convenient. It would be much shorter without the color change, but it would be less fun and less nice!
		    ImGui::PushID(row*2+30);
		    if (mkeys[row].active1) {
			prepare_red(red);
			popstyle = true;
		    }

		    if (ImGui::Button(SDL_GetScancodeName((SDL_Scancode)mkeys[row].key1), ImVec2(-FLT_MIN, 0.0f))) {
			mkeys[row].active1 = true;
			capturing_key = true;
			captured_key = 0;
		    }
		    if (popstyle) {
			ImGui::PopStyleColor(3);
			popstyle = false;
		    }
		    ImGui::PopID();
		    if (mkeys[row].active1 && captured_key) {
			mkeys[row].active1 = false;
			capturing_key = false;
			mkeys[row].key1 = captured_key;
			keyxlate.setkey(captured_key, mkeys[row].key, TYPEMSCANL, 0);
			captured_key = 0;
		    }
		}
		ImGui::EndTable();
	    }
	    ImGui::End();
	}
    }

    if (listing && listing_title && show_listing) {
	if (ImGui::Begin(listing_title, &show_listing)) {
	    if (ImGui::Button(_("Save to report.txt"))) {
		FILE *f = fopen("report.txt","w");
		if (f) {
		    fprintf(f,"%s\n",listing->m_listing);
		    fclose(f);
		}
	    }

	    if (ImGui::IsWindowHovered()) {
		SDL_ShowCursor(SDL_ENABLE);
		imgui_active = true;
	    }
	    ImGui::Text("%s",listing->m_listing);
	    ImGui::End();
	} else {
	    printf("window collapsed or closed\n");
	    ImGui::End();
	}
    }

    if (d.PartyHasDied)
	open_char_info = false;

    if (open_char_info) {
	static int selected_item = 0;
	char *name[4];
	for (int n=0; n<d.NumCharacter; n++)
	    name[n] = d.hero[n].name;
        const char* combo_preview_value = name[selected_item];

	if (ImGui::Begin("Character info",&open_char_info,ImGuiWindowFlags_AlwaysAutoResize)) {
	    if (ImGui::IsWindowHovered()) {
		SDL_ShowCursor(SDL_ENABLE);
		imgui_active = true;
	    }
	    if (ImGui::BeginCombo("##combo1", combo_preview_value))
	    {
		imgui_active = true;
		for (int n = 0; n < d.NumCharacter; n++)
		{
		    const bool is_selected = (selected_item == n);
		    if (ImGui::Selectable(name[n], is_selected))
			selected_item = n;

		    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
		    if (is_selected)
			ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	    }

	    ImGui::BeginTable("##table1",2);
	    for (int n=0; n<4; n++)
		handle_skill(n,d.hero[selected_item].skills92[n].experience);
	    ImGui::EndTable();
	    if (ImGui::CollapsingHeader("Hidden skills")) {
		ImGui::BeginTable("##table2",2);
		for (int n=4; n<20; n++)
		    handle_skill(n,d.hero[selected_item].skills92[n].experience);
		ImGui::EndTable();
	    }
	    char buf[80];
	    sprintf(buf,"Luck %d",d.hero[selected_item].Attributes[0].Current());
	    ImGui::Text(buf);

	    ImGui::Text("Defense:");
	    for (int slot=0; slot<=5; slot++) {
		int def = get_def(selected_item,1<<slot);
		const char *slots[] = { "ready hand", "action hand", "head", "body", "legs", "feet" };
		sprintf(buf,"%s: %d",slots[slot],def);
		ImGui::Text(buf);
	    }

	    ImGui::End();
	} else {
	    // reduced or closed
	    ImGui::End();
	}
    }

    if (ImGui::BeginPopupModal("Question", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
	ImGui::Text(popup_msg);
	ImGui::Separator();

	//static int unused_i = 0;
	//ImGui::Combo("Combo", &unused_i, "Delete\0Delete harder\0");

	if (ImGui::Button(_("Yes"), ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter)) { ImGui::CloseCurrentPopup(); yes_selected = 1; want_popup = 0; }
	ImGui::SameLine();
	if (ImGui::Button(_("No"), ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) { ImGui::CloseCurrentPopup(); want_popup = 0; }
	ImGui::EndPopup();
    }

    if (experimental_overlay)
	render_overlay_interface();

    if (!was_active && !cursorIsShowing && !fb_shown && !imgui_active && !fb2_shown && !fb3_shown) {
	hide_system_pointer();
    }

    // Rendering
    ImGui::Render();

    SDL_RenderSetScale(sdlRenderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(sdlRenderer);

    double ratio = r.w*1.0/r.h;
    if (abs(ratio - 32/20.0) > 1e-5) {
	if (ratio > 32/20.0) {
	    r.w = r.h*32/20.0;
	    r.x = (WindowWidth-r.w)/2;
	} else {
	    r.h = r.w*20/32.0;
	    r.y = (WindowHeight - r.h)/2;
	}
    }
    if (d.NumGraphic>0 && SDL_RenderCopy(sdlRenderer,
		sdlTexture,
		NULL,
		&r) < 0) {
	printf("rendercopy error %s\n",SDL_GetError());
    }
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),sdlRenderer);
    SDL_RenderPresent(sdlRenderer);
    if (want_popup)
	imgui_active = 1;
}
