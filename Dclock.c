/*
 * Dclock.c (v.2.0) -- a digital clock widget.
 * Copyright (c) 1988 Dan Heller <argv@sun.com>
 * Modifications 2/93 by Tim Edwards <tim@sinh.stanford.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <values.h>
#include <math.h>
#include <time.h>
#include <wait.h>
#include <errno.h>
#include <X11/IntrinsicP.h>
#include <X11/Xos.h>
#include <X11/StringDefs.h>
#include "DclockP.h"

static void
    Initialize(), Resize(), ResizeNow(), Realize(), Destroy(), Redisplay(),
    GetGC(), invert_bitmap(), build_segments(), timeout(), toggle_bell(),
    toggle_reverse_video(), toggle_scroll(), toggle_seconds(), show_bell(),
    toggle_military_time(), toggle_date(), make_number(), make_number_one(),
    set_alarm(),
    show_date(), show_alarm(), scroll_time(), toggle_alarm(), outline_digit(),
    toggle_fade(), toggle_tails(), increase_slope(), decrease_slope(),
    widen_segment(), thin_segment(), increase_space(), decrease_space(),
    toggle_blink(), print_help(), print_dump(), playbell(), toggle_dateup();

static void XfFillArc(), XfFillPolygon();

/* some definitions */

#define BORDER		5
#define CLOCK_WIDTH	220
#define CLOCK_HEIGHT	80
#define DATE_FMT	"%A, %B %d, %Y"
#define when		break;case
#define otherwise	break;default

#define sign(a)         ((a) < 0.0 ? -1 : 1)
#define abs(a)          ((a) < 0 ? -(a) : (a))

/* the x-value of the colon as measured from the window x=0 */

#define cx (int)(digit_w + (w->dclock.miltime ? digit_w : \
	w->dclock.space_factor * 2 * digit_w + segxwidth))

/* A few variables and initial values */

static Boolean SetValues(), show_time();
static Dimension winwidth = CLOCK_WIDTH;
static Dimension winheight = CLOCK_HEIGHT;
static Boolean false = False;
static Boolean true = True;
static int fade_rate = 50;
static float y_ratio;
static float sslope = 6.0;
static float sspacer = 0.09;
static float smallrat = 0.67;
static float secgap = 0.3;
static float widfac = 0.13;
static Pixmap old_pix[4];
static int old_digs[4];
static struct tm before;
static char *saved_date;
static cur_position;	/* outline current digit for setting alarm */
static struct { int hrs, mins; } Alarm;
static int TopOffset = 0;

/* define what keys do what when pressed in the window */

static char defaultTranslations[] =
    "<Key>b:		toggle-bell()		\n\
     <Key>j:		toggle-scroll()		\n\
     <Key>f:		toggle-fade()		\n\
     <Key>s:		toggle-seconds()	\n\
     <Key>r:		toggle-reverse-video()	\n\
     <Key>colon:	toggle-blink()		\n\
     <Key>m:		toggle-military-time()	\n\
     <Key>d:		toggle-date()		\n\
     <Key>u:		toggle-dateup()		\n\
     <Key>t:		toggle-tails()		\n\
     <Key>a:		toggle-alarm()		\n\
     <Key>h:		print-help()		\n\
     Shift<Key>slash:	print-help()		\n\
     <Key>v:		print-dump()		\n\
     <Key>+:		widen-segment()		\n\
     <Key>-:		thin-segment()		\n\
     <Key>slash:	slope-increase()	\n\
     <Key>backslash:	slope-decrease()	\n\
     Shift<Key>comma:	space-decrease()	\n\
     Shift<Key>period:	space-increase()	\n\
     <BtnDown>:		set-alarm()";

static XtActionsRec actionsList[] = {
    { "toggle-bell",		toggle_bell		},
    { "toggle-scroll",		toggle_scroll		},
    { "toggle-fade",		toggle_fade		},
    { "toggle-seconds",		toggle_seconds		},
    { "toggle-reverse-video",	toggle_reverse_video	},
    { "toggle-blink",		toggle_blink		},
    { "toggle-military-time",	toggle_military_time	},
    { "toggle-date",		toggle_date		},
    { "toggle-dateup",		toggle_dateup		},
    { "toggle-tails",		toggle_tails		},
    { "toggle-alarm",		toggle_alarm		},
    { "widen-segment",		widen_segment		},
    { "thin-segment",		thin_segment		},
    { "slope-decrease",		decrease_slope		},
    { "slope-increase",		increase_slope		},
    { "space-decrease",		decrease_space		},
    { "space-increase",		increase_space		},
    { "set-alarm",		set_alarm		},
    { "print-help",		print_help		},
    { "print-dump",		print_dump		},
};

/* Initialization values for all of the X resources */

static XtResource resources[] = {
    { XtNwidth, XtCWidth, XtRDimension, sizeof(Dimension),
	XtOffset(Widget,core.width), XtRDimension, (caddr_t)&winwidth },
    { XtNheight, XtCHeight, XtRDimension, sizeof(Dimension),
	XtOffset(Widget,core.height), XtRDimension, (caddr_t)&winheight },
    { XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
        XtOffset(DclockWidget,dclock.foreground), XtRString, "Chartreuse"},
    { XtNled_off, XtCForeground, XtRPixel, sizeof(Pixel),
        XtOffset(DclockWidget,dclock.led_off), XtRString, "DarkGreen"},
    { XtNbackground, XtCBackground, XtRPixel, sizeof(Pixel),
        XtOffset(DclockWidget,dclock.background), XtRString, "DarkSlateGray"},
    { XtNtails, XtCBoolean, XtRBoolean, sizeof (Boolean),
	XtOffset(DclockWidget,dclock.tails), XtRBoolean, (caddr_t)&true},
    { XtNfade, XtCBoolean, XtRBoolean, sizeof (Boolean),
	XtOffset(DclockWidget,dclock.fade), XtRBoolean, (caddr_t)&false},
    { XtNfadeRate, XtCTime, XtRInt, sizeof (int),
	XtOffset(DclockWidget,dclock.fade_rate), XtRInt, (caddr_t)&fade_rate},
    { XtNscroll, XtCBoolean, XtRBoolean, sizeof (Boolean),
	XtOffset(DclockWidget,dclock.scroll), XtRBoolean, (caddr_t)&false},
    { XtNdisplayTime, XtCBoolean, XtRBoolean, sizeof (Boolean),
	XtOffset(DclockWidget,dclock.display_time), XtRBoolean, (caddr_t)&true},
    { XtNalarm, XtCBoolean, XtRBoolean, sizeof (Boolean),
	XtOffset(DclockWidget,dclock.alarm), XtRBoolean, (caddr_t)&false},
    { XtNalarmTime, XtCTime, XtRString, sizeof (char *),
	XtOffset(DclockWidget,dclock.alarm_time), XtRString, "00:00" },
    { XtNalarmPersist, XtCBoolean, XtRBoolean, sizeof (Boolean),
	XtOffset(DclockWidget,dclock.alarm_persist), XtRBoolean, (caddr_t)&false},
    { XtNbell, XtCBoolean, XtRBoolean, sizeof (Boolean),
	XtOffset(DclockWidget,dclock.bell), XtRBoolean, (caddr_t)&false},
    { XtNseconds, XtCBoolean, XtRBoolean, sizeof (Boolean),
	XtOffset(DclockWidget,dclock.seconds), XtRBoolean, (caddr_t)&true},
    { XtNblink, XtCBoolean, XtRBoolean, sizeof (Boolean),
	XtOffset(DclockWidget,dclock.blink), XtRBoolean, (caddr_t)&true},
    { XtNmilitaryTime, XtCBoolean, XtRBoolean, sizeof (Boolean),
	XtOffset(DclockWidget,dclock.miltime), XtRBoolean, (caddr_t)&true},
    { XtNutcTime, XtCBoolean, XtRBoolean, sizeof (Boolean),
	XtOffset(DclockWidget,dclock.utc), XtRBoolean, (caddr_t)&false},
    { XtNdate, XtCString, XtRString, sizeof (String),
	XtOffset(DclockWidget,dclock.date_fmt), XtRString, NULL},
    { XtNdateUp, XtCBoolean, XtRBoolean, sizeof (Boolean),
	XtOffset(DclockWidget,dclock.dateup), XtRBoolean, (caddr_t)&false},
    { XtNfont, XtCFont, XtRFontStruct, sizeof (XFontStruct *),
	XtOffset(DclockWidget,dclock.font), XtRString, "fixed"},
#ifdef XFT_SUPPORT
    { XftNfontName, "fontName", XtRString, sizeof(String),
	XtOffset(DclockWidget,dclock.xftfontname), XtRString, "charter"},
#endif
    { XtNangle, "Slope", XtRFloat, sizeof(float),
	XtOffset(DclockWidget,dclock.angle), XtRFloat, (caddr_t)&sslope},
    { XtNwidthFactor, "Ratio", XtRFloat, sizeof(float),
	XtOffset(DclockWidget,dclock.width_factor), XtRFloat, (caddr_t)&widfac},
    { XtNsmallRatio, "Ratio", XtRFloat, sizeof(float),
	XtOffset(DclockWidget,dclock.small_ratio), XtRFloat,(caddr_t)&smallrat},
    { XtNsecondGap, "Ratio", XtRFloat, sizeof(float),
	XtOffset(DclockWidget,dclock.sec_gap), XtRFloat, (caddr_t)&secgap}, 
    { XtNspaceFactor, "Ratio", XtRFloat, sizeof(float),
	XtOffset(DclockWidget,dclock.space_factor), XtRFloat,(caddr_t)&sspacer},
    { XtNalarmFile, "alarmFile", XtRString, sizeof(String),
	XtOffset(DclockWidget,dclock.alarmfile), XtRString, (String)NULL},
    { XtNbellFile, "bellFile", XtRString, sizeof(String),
	XtOffset(DclockWidget,dclock.bellfile), XtRString, (String)NULL},
    { XtNaudioPlay, "audioPlay", XtRString, sizeof(String),
	XtOffset(DclockWidget,dclock.audioplay), XtRString, "/usr/bin/play"},
};

/* Define the Dclock widget */

DclockClassRec dclockClassRec = {
    { /* core fields */
    /* superclass		*/	&widgetClassRec,
    /* class_name		*/	"Dclock",
    /* widget_size		*/	sizeof(DclockRec),
    /* class_initialize		*/	NULL,
    /* class_part_initialize	*/	NULL,
    /* class_inited		*/	FALSE,
    /* initialize		*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize			*/	Realize,
    /* actions			*/	actionsList,
    /* num_actions		*/	XtNumber(actionsList),
    /* resources		*/	resources,
    /* resource_count		*/	XtNumber(resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	Destroy,
    /* resize			*/	Resize,
    /* expose			*/	Redisplay,
    /* set_values		*/	SetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	defaultTranslations,
    /* query_geometry		*/	NULL,
    }
};
WidgetClass dclockWidgetClass = (WidgetClass) &dclockClassRec;


#ifdef NO_USLEEP
#define usleep(x)	{ struct timeval st_delay;			\
			  st_delay.tv_usec = (x); st_delay.tv_sec = 0;	\
			  select(32, NULL, NULL, NULL, &st_delay); }
#endif


/*
 * These stipples give different densities for the
 * different stages of fading.
 */
static unsigned char stpl_1_8th[] =
{
    0x80, 0x80, 0x08, 0x08, 0x80, 0x80, 0x08, 0x08,
    0x80, 0x80, 0x08, 0x08, 0x80, 0x80, 0x08, 0x08,
    0x80, 0x80, 0x08, 0x08, 0x80, 0x80, 0x08, 0x08,
    0x80, 0x80, 0x08, 0x08, 0x80, 0x80, 0x08, 0x08
};

static unsigned char stpl_1_4th[] =
{
    0x88, 0x88, 0x22, 0x22, 0x88, 0x88, 0x22, 0x22,
    0x88, 0x88, 0x22, 0x22, 0x88, 0x88, 0x22, 0x22,
    0x88, 0x88, 0x22, 0x22, 0x88, 0x88, 0x22, 0x22,
    0x88, 0x88, 0x22, 0x22, 0x88, 0x88, 0x22, 0x22
};

static unsigned char stpl_3_8ths[] =
{
    0xA2, 0xA2, 0x15, 0x15, 0xA8, 0xA8, 0x45, 0x45,
    0x2A, 0x2A, 0x51, 0x51, 0x8A, 0x8A, 0x54, 0x54,
    0xA2, 0xA2, 0x15, 0x15, 0xA8, 0xA8, 0x45, 0x45,
    0x2A, 0x2A, 0x51, 0x51, 0x8A, 0x8A, 0x54, 0x54
};

static unsigned char stpl_one_half[] =
{
    0x55, 0x55, 0xAA, 0xAA, 0x55, 0x55, 0xAA, 0xAA,
    0x55, 0x55, 0xAA, 0xAA, 0x55, 0x55, 0xAA, 0xAA,
    0x55, 0x55, 0xAA, 0xAA, 0x55, 0x55, 0xAA, 0xAA,
    0x55, 0x55, 0xAA, 0xAA, 0x55, 0x55, 0xAA, 0xAA
};

/*
 * fade_stip[] eventually contains the pixmaps used for each
 * iteration of fading (initialized in Initialize()).
 * If smooth color variations are used instead, then the
 * colormap index values are held in fadevector[].
 */
#define FADE_ITER	8
static Pixmap fade_stip[FADE_ITER];
int fadevector[FADE_ITER];
int bsave = 0;
Boolean use_stipple;

#define MAX_PTS		6	/* max # of pts per segment polygon */
#define NUM_SEGS	7	/* number of segments in a digit */

/*
 * These constants give the bit positions for the segmask[]
 * digit masks.
 */
#define TOP		0
#define MIDDLE		1
#define BOTTOM		2
#define TOP_LEFT	3
#define BOT_LEFT	4
#define TOP_RIGHT	5
#define BOT_RIGHT	6

#define msk(i)		(1 << (i))

/*
 * segmask[n] contains a bitmask of the segments which
 * should be lit for digit 'n'.
 */
int segmask[11] =
{
/* 0 */ msk(TOP) | msk(TOP_RIGHT) | msk(BOT_RIGHT) | msk(BOTTOM)
	  | msk(BOT_LEFT) | msk(TOP_LEFT),
/* 1 */	msk(TOP_RIGHT) | msk(BOT_RIGHT),
/* 2 */ msk(TOP) | msk(TOP_RIGHT) | msk(MIDDLE) | msk(BOT_LEFT)
	  | msk(BOTTOM),
/* 3 */ msk(TOP) | msk(TOP_RIGHT) | msk(MIDDLE) | msk(BOT_RIGHT)
	  | msk(BOTTOM),
/* 4 */ msk(TOP_LEFT) | msk(MIDDLE) | msk(TOP_RIGHT) | msk(BOT_RIGHT),
/* 5 */ msk(TOP) | msk(TOP_LEFT) | msk(MIDDLE) | msk(BOT_RIGHT)
	  | msk(BOTTOM),
/* 6 */ msk(TOP_LEFT) | msk(BOT_LEFT) | msk(MIDDLE) | msk(BOTTOM)
	  | msk(BOT_RIGHT),
/* 7 */ msk(TOP) | msk(TOP_RIGHT) | msk(BOT_RIGHT),
/* 8 */ msk(TOP_LEFT) | msk(BOT_LEFT) | msk(MIDDLE) | msk(TOP)
	  | msk(BOTTOM) | msk(TOP_RIGHT) | msk(BOT_RIGHT),
/* 9 */ msk(TOP) | msk(TOP_LEFT) | msk(TOP_RIGHT) | msk(MIDDLE)
	  | msk(BOT_RIGHT),
/* blank */ 0
};

/*
 * num_segment_pts indicates the number of vertices on the
 * polygon that describes a segment
 */
static int num_segment_pts = 6;
float slope_add, segxwidth, sxw;

/* Clipping masks to prevent angled digits from obscuring one another */

Region clip_norm, clip_small, clip_colon; 
XPoint clip_pts[5];

typedef struct {
   float x;
   float y;
} XfPoint;

typedef XfPoint segment_pts[NUM_SEGS][MAX_PTS];

segment_pts tiny_segment_pts;
segment_pts norm_segment_pts;

/* ARGSUSED */
static void
Initialize (request, new)
DclockWidget   request;
DclockWidget   new;
{
    int i, n;
    Display *dpy = XtDisplay(new);
    Drawable root;
    Colormap cmap = DefaultColormap (dpy, DefaultScreen(dpy));
    XColor cvcolor, cvfg, cvbg;
    float frac;

    root = RootWindow(dpy, DefaultScreen(dpy));

    if (new->dclock.alarm_time) {
	if (sscanf(new->dclock.alarm_time, "%2d:%2d",
		&Alarm.hrs, &Alarm.mins) != 2 || Alarm.hrs >= 24 ||
		Alarm.mins >= 60) {
	    XtWarning("Alarm Time is in incorrect format.");
	    new->dclock.alarm_time = "00:00";
	    Alarm.hrs = Alarm.mins = 0;
	}
	new->dclock.alarm_time =
	    strcpy(XtMalloc(strlen(new->dclock.alarm_time)+1),
				   new->dclock.alarm_time);
    }

    /* control allowable features */

    if (new->dclock.fade) new->dclock.scroll = False;
    if (new->dclock.angle < 1.0) new->dclock.angle = 1.0;    
    if (new->dclock.angle > 2 * winheight) new->dclock.angle = 2 * winheight;
    if (new->dclock.space_factor > 0.8) new->dclock.space_factor = 0.8;
    if (new->dclock.space_factor < 0.0) new->dclock.space_factor = 0.0;
    if (new->dclock.small_ratio < 0.1) new->dclock.small_ratio = 0.1;
    if (new->dclock.small_ratio > 1.0) new->dclock.small_ratio = 1.0;
    if (new->dclock.width_factor > 0.24) new->dclock.width_factor = 0.24;
    if (new->dclock.width_factor < 0.01) new->dclock.width_factor = 0.01;
    if (new->dclock.sec_gap < 0.0) new->dclock.sec_gap = 0.0;

#ifdef XFT_SUPPORT
    new->dclock.xftfont = XftFontOpen(dpy, DefaultScreen(dpy),
		XFT_FAMILY, XftTypeString, new->dclock.xftfontname,
		XFT_SIZE, XftTypeDouble, ((float)new->core.height / 6.0) - 4.0,
		NULL);
    new->dclock.xftdraw = NULL;
#endif

    GetGC(new);

    /* Get RGB values for the foreground and background */

    cvfg.pixel = new->dclock.foreground;
    XQueryColor(dpy, cmap, &cvfg);
    cvbg.pixel = new->dclock.led_off;
    XQueryColor(dpy, cmap, &cvbg);

    /* Attempt to initialize a color vector of FADE_ITER points between */
    /* foreground and background */

    use_stipple = False;
    for (i = 0; i < FADE_ITER; i++) {
       frac = (float)i / (float)(FADE_ITER - 1);
       /* gamma correction */
       frac = pow(frac, 0.5);

       cvcolor.green = cvbg.green + (int)((float)(cvfg.green - cvbg.green) * frac);
       cvcolor.blue = cvbg.blue + (int)((float)(cvfg.blue - cvbg.blue) * frac);
       cvcolor.red = cvbg.red + (int)((float)(cvfg.red - cvbg.red) * frac);

       if (!XAllocColor(dpy, cmap, &cvcolor)) {
	  use_stipple = True;
	  break;
       }
       fadevector[i] = cvcolor.pixel;
    }

    if (use_stipple) {
       fade_stip[1] = XCreateBitmapFromData(dpy, root, stpl_1_8th, 16, 16);
       fade_stip[2] = XCreateBitmapFromData(dpy, root, stpl_1_4th, 16, 16);
       fade_stip[3] = XCreateBitmapFromData(dpy, root, stpl_3_8ths, 16, 16);
       fade_stip[4] = XCreateBitmapFromData(dpy, root, stpl_one_half, 16, 16);
       invert_bitmap(stpl_1_8th, 16, 16);
       invert_bitmap(stpl_1_4th, 16, 16);
       invert_bitmap(stpl_3_8ths, 16, 16);
       fade_stip[5] = XCreateBitmapFromData(dpy, root, stpl_3_8ths, 16, 16);
       fade_stip[6] = XCreateBitmapFromData(dpy, root, stpl_1_4th, 16, 16);
       fade_stip[7] = XCreateBitmapFromData(dpy, root, stpl_1_8th, 16, 16);

       for (n = 1; n != FADE_ITER; ++n)
	   if (!fade_stip[n])
	   {
	       fprintf(stderr, "dclock: Couldn't create stipple!\n");
	       exit(1);
	   }
    }

    if (!new->dclock.date_fmt || !*new->dclock.date_fmt)
	saved_date = DATE_FMT;
    if (new->dclock.date_fmt && !*new->dclock.date_fmt)
	new->dclock.date_fmt = NULL;
    if (new->dclock.dateup && new->dclock.date_fmt)
#ifdef XFT_SUPPORT
	TopOffset = new->core.height / 6;
#else
	TopOffset = new->dclock.font->ascent + new->dclock.font->descent;
#endif

    else
	TopOffset = 0;

    /* Shouldn't be necessary, but play it safe anyway */
    for (n = 0; n < 10; n++)
    {
	new->dclock.digits[n] = 0;
	new->dclock.tiny_digits[n] = 0;
    }
    new->dclock.digit_one[0] = new->dclock.digit_one[1] = 0;
    new->dclock.colon[0] = new->dclock.colon[1] = 0;

    new->dclock.interval_id = (XtIntervalId)NULL;
    new->dclock.punt_resize = (XtIntervalId)NULL;

}

static void
GetGC(w)
DclockWidget w;
{ XGCValues  	xgcv;
    XtGCMask	gc_mask =
		    GCGraphicsExposures | GCFont | GCForeground | GCBackground;

    xgcv.font = w->dclock.font->fid;
    xgcv.graphics_exposures = FALSE;
    xgcv.foreground = w->dclock.foreground;
    xgcv.background = w->dclock.background;

    w->dclock.foreGC = XtGetGC ((Widget) w, gc_mask, &xgcv);

    xgcv.foreground = w->dclock.background;
    xgcv.background = w->dclock.foreground;

    w->dclock.backGC = XtGetGC ((Widget) w, gc_mask, &xgcv);
}


static void
invert_bitmap(bm, h, w)
unsigned char *bm;
int h, w;
{
    int i, *wp;

    for (wp = (int *) bm, i = (h*w) / (8*sizeof(int)); i != 0; --i, ++wp)
	*wp = ~(*wp);
}

static void
Realize (w, valueMask, attrs)
Widget w;
XtValueMask *valueMask;
XSetWindowAttributes *attrs;
{
    Display *dp = XtDisplay(w); 
    DclockWidget dw = (DclockWidget)w;
    int sc = DefaultScreen(dp);
    Visual *vs = DefaultVisual(dp, sc);;

    *valueMask |= CWBitGravity;
    attrs->bit_gravity = ForgetGravity;
    vs = DefaultVisual(dp, sc);
    XtCreateWindow(w, InputOutput, (Visual *)CopyFromParent, *valueMask, attrs);

#ifdef XFT_SUPPORT
    if (dw->dclock.xftdraw == NULL) {
	XColor color;
	Colormap cm = DefaultColormap(dp, sc);

	dw->dclock.xftdraw = XftDrawCreate(dp, XtWindow(w), vs, cm);

	color.pixel = dw->dclock.foreground;
	XQueryColor(dp, cm, &color);
	dw->dclock.xftfg.color.red = color.red;
	dw->dclock.xftfg.color.green = color.green;
	dw->dclock.xftfg.color.blue = color.blue;
	dw->dclock.xftfg.color.alpha = 0xffff;
	dw->dclock.xftfg.pixel = color.pixel;

	color.pixel = dw->dclock.background;
	XQueryColor(dp, cm, &color);
	dw->dclock.xftbg.color.red = color.red;
	dw->dclock.xftbg.color.green = color.green;
	dw->dclock.xftbg.color.blue = color.blue;
	dw->dclock.xftbg.color.alpha = 0xffff;
	dw->dclock.xftbg.pixel = color.pixel;
    }
#endif
    ResizeNow(w);
}

static void
Destroy (w)
DclockWidget w;
{
    int n;

    /* Be nice and free up memory before exiting. */

    if (w->dclock.interval_id != (XtIntervalId)NULL)
	XtRemoveTimeOut(w->dclock.interval_id);
    if (w->dclock.punt_resize != (XtIntervalId)NULL)
	XtRemoveTimeOut(w->dclock.punt_resize);
    XtReleaseGC ((Widget)w, w->dclock.foreGC);
    XtReleaseGC ((Widget)w, w->dclock.backGC);
    for (n = 0; n < 10; n++) {
	XFreePixmap(XtDisplay(w), w->dclock.digits[n]);
	XFreePixmap(XtDisplay(w), w->dclock.tiny_digits[n]);
    }
    XFreePixmap(XtDisplay(w), w->dclock.digit_one[0]);
    XFreePixmap(XtDisplay(w), w->dclock.digit_one[1]);
    if (w->dclock.colon[0])
	XFreePixmap(XtDisplay(w), w->dclock.colon[0]);
    if (w->dclock.colon[1])
	XFreePixmap(XtDisplay(w), w->dclock.colon[1]);
    XDestroyRegion(clip_norm);
    XDestroyRegion(clip_small);
    XDestroyRegion(clip_colon);

#ifdef XFT_SUPPORT
    XftFontClose(XtDisplay(w), w->dclock.xftfont);
#endif
}

/* ARGSUSED */
static void
ResizeNow  (w)
DclockWidget    w;
{
    int i, j; 
    float digit_w, digit_h, seg_width;
    Pixmap pix;
    GC gc = w->dclock.foreGC;
    Display *dpy = XtDisplay(w);

    if (!XtIsRealized((Widget)w))
	return;

    winwidth = w->core.width;
    winheight = w->core.height;

#ifdef XFT_SUPPORT
    XftFontClose(dpy, w->dclock.xftfont);
    w->dclock.xftfont = XftFontOpen(dpy, DefaultScreen(dpy),
		XFT_FAMILY, XftTypeString, w->dclock.xftfontname,
		XFT_SIZE, XftTypeDouble, ((float)winheight / 6.0) - 4.0,
		NULL);
#endif

    y_ratio = (float)winheight / CLOCK_HEIGHT;

    if (w->dclock.date_fmt || !w->dclock.display_time || w->dclock.alarm ||
	w->dclock.bell)
	/* make win temporarily shorter so digits will fit on top of date */
#ifdef XFT_SUPPORT
	winheight -= w->core.height / 6;
#else
	winheight -= w->dclock.font->ascent + w->dclock.font->descent;
#endif

    /*
     * if the width of all segments are equal, then the width in x is the
     * segment width times the cosine of the slope angle.
     */
    sxw = (w->dclock.width_factor * 
	sqrt(1 + (w->dclock.angle)*(w->dclock.angle)))/(w->dclock.angle);

    /* height of the digit, leaving some room for a border */
    digit_h = winheight - y_ratio * BORDER*2;

    /* the amount of skew measured along x due to the slope of the digits */
    slope_add = digit_h / (w->dclock.angle); 

    /* calculate the width of the digit based on everything. */
    /*   All attributes are calculated as ratios to the digit width. */
    /*   The colon is 1/2 the digit width; */
    /*   All other ratios are widget attributes. */

    digit_w = (winwidth - slope_add * !(w->dclock.seconds) -
	(slope_add * w->dclock.small_ratio) * w->dclock.seconds) / (3.5 + 
	w->dclock.miltime + w->dclock.seconds * (3 * w->dclock.small_ratio 
	+ w->dclock.sec_gap) + (3 * w->dclock.space_factor + sxw) * 
	!(w->dclock.miltime));

    /* segment widths as measured in x and y */

    segxwidth = sxw * digit_w;
    seg_width = w->dclock.width_factor * (digit_w + slope_add);
    
    /* create the clipping masks */

    clip_pts[0].x = clip_pts[4].x = 0;
    clip_pts[1].x = (int)(slope_add);
    clip_pts[2].x = (int)(slope_add + digit_w);
    clip_pts[3].x = (int)digit_w;
    clip_pts[1].y = clip_pts[2].y = 0;
    clip_pts[0].y = clip_pts[4].y = clip_pts[3].y = digit_h;
    clip_norm = XPolygonRegion(clip_pts, 5, WindingRule);
    
    for(i = 0; i < 5; i++) {
	clip_pts[i].x = (int)((float)clip_pts[i].x * w->dclock.small_ratio);
	clip_pts[i].y = (int)((float)clip_pts[i].y * w->dclock.small_ratio);
    }
    clip_small = XPolygonRegion(clip_pts, 5, WindingRule);

    clip_pts[1].x = (int)slope_add;
    clip_pts[2].x = (int)(slope_add + 0.5 * digit_w);
    clip_pts[3].x = (int)(0.5 * digit_w);
    clip_pts[0].y = clip_pts[4].y = clip_pts[3].y = digit_h;
    clip_colon = XPolygonRegion(clip_pts, 5, WindingRule);

/*  printf("segxwidth = %f\n", segxwidth);
    printf("winwidth = %d\n", winwidth);
    printf("digit_w = %f\n", digit_w);   */

    w->dclock.digit_w = digit_w;
    w->dclock.digit_h = digit_h;

    if (w->dclock.tails)
    {
	segmask[6] |=  msk(TOP);
	segmask[9] |=  msk(BOTTOM);
    }
    else
    {
	segmask[6] &=  ~msk(TOP);
	segmask[9] &=  ~msk(BOTTOM);
    }

    build_segments(w, norm_segment_pts, slope_add + digit_w, digit_h);

/*      printf("Tiny Segment Relative Points Dump:\n");
        for (i = 0; i < 7; i++) 
	   for (j = 0; j < 6; j++)
	      printf("digit[%d] point[%d]: x=%d, y=%d\n", i, j, 
		tiny_segment_pts[i][j].x, tiny_segment_pts[i][j].y); 
*/

    if (w->dclock.seconds)
	build_segments(w, tiny_segment_pts, w->dclock.small_ratio * (slope_add 
	   + digit_w), w->dclock.small_ratio * digit_h);

    for (i = 0; i < 10; i++) {

	/* Make the big digit */

	if (w->dclock.digits[i])
	    XFreePixmap(XtDisplay(w), w->dclock.digits[i]);
	w->dclock.digits[i] =
	      XCreatePixmap(XtDisplay(w), XtWindow(w), (int)(slope_add + 
		digit_w), (int)digit_h, DefaultDepthOfScreen(XtScreen(w)));
	XFillRectangle(XtDisplay(w), w->dclock.digits[i], w->dclock.backGC, 
		0, 0, (int)(slope_add + digit_w), (int)digit_h);
	make_number(w, w->dclock.digits[i], gc, i, norm_segment_pts);

	/* make smaller version of this digit for use by "seconds" */

	if (w->dclock.tiny_digits[i])
	    XFreePixmap(XtDisplay(w), w->dclock.tiny_digits[i]);
	if (w->dclock.seconds)
	{
	    w->dclock.tiny_digits[i] =
			XCreatePixmap(XtDisplay(w), XtWindow(w), (int)(
					(slope_add + digit_w) * 
					w->dclock.small_ratio),
					(int)(w->dclock.small_ratio * digit_h),
					DefaultDepthOfScreen(XtScreen(w)));
	    XFillRectangle(XtDisplay(w), w->dclock.tiny_digits[i],
			   w->dclock.backGC, 0, 0, (int)((slope_add + 
				digit_w) * w->dclock.small_ratio),
				(int)(w->dclock.small_ratio * digit_h));
	    make_number(w, w->dclock.tiny_digits[i], gc, i, tiny_segment_pts);
	}
	else
	    w->dclock.tiny_digits[i] = (Pixmap)NULL;
    }

    /* digit_one is either the number "1" or a blank */

    if (w->dclock.digit_one[1])
	XFreePixmap(XtDisplay(w), w->dclock.digit_one[1]);
    w->dclock.digit_one[1] =
		XCreatePixmap(XtDisplay(w), XtWindow(w), (int)(slope_add +
		digit_w), (int)digit_h, DefaultDepthOfScreen(XtScreen(w)));
    XFillRectangle(XtDisplay(w), w->dclock.digit_one[1], w->dclock.backGC,
		0, 0, (int)(slope_add + digit_w), (int)digit_h);
    make_number_one(w, w->dclock.digit_one[1], gc, 1, norm_segment_pts);

    if (w->dclock.digit_one[0])
	XFreePixmap(XtDisplay(w), w->dclock.digit_one[0]);
    w->dclock.digit_one[0] =
		XCreatePixmap(XtDisplay(w), XtWindow(w), (int)(slope_add + 
		digit_w), (int)digit_h, DefaultDepthOfScreen(XtScreen(w)));
    XFillRectangle(XtDisplay(w), w->dclock.digit_one[0], w->dclock.backGC, 
		0, 0, (int)(slope_add + digit_w), (int)digit_h);
    make_number_one(w, w->dclock.digit_one[0], gc, 10, norm_segment_pts);

    /* The colon[0] area is blank (segment "off" color) */
    
    if (w->dclock.colon[0])
	XFreePixmap(XtDisplay(w), w->dclock.colon[0]);
    w->dclock.colon[0] = XCreatePixmap(XtDisplay(w), XtWindow(w),
		(int)(slope_add + 0.5 * digit_w), (int)digit_h,
		DefaultDepthOfScreen(XtScreen(w)));

    XFillRectangle(XtDisplay(w), w->dclock.colon[0],
	w->dclock.backGC, 0, 0, (int)(slope_add + 0.5 * digit_w), (int)digit_h);

    /* The additive 2 for the Arc diameter keeps the colon from looking too
       small at small display sizes. */

    XfFillArc(XtDisplay(w), w->dclock.colon[0], gc,
		(slope_add + digit_w) * 0.25 - 0.5, 0.75 * digit_h - 0.5,
		1 + seg_width/2, 0);
    XfFillArc(XtDisplay(w), w->dclock.colon[0], gc,
		slope_add * 0.75 + digit_w * 0.25 - 0.5, 0.25 * digit_h - 0.5,
		1 + seg_width/2, 0);

    /* colon[1] area has two circles */

    if (w->dclock.colon[1])
	XFreePixmap(XtDisplay(w), w->dclock.colon[1]);
    w->dclock.colon[1] = XCreatePixmap(XtDisplay(w), XtWindow(w),
		(int)(slope_add + 0.5 * digit_w), (int)digit_h,
		DefaultDepthOfScreen(XtScreen(w)));

    XFillRectangle(XtDisplay(w), w->dclock.colon[1],
    	w->dclock.backGC, 0, 0, (int)(slope_add + 0.5 * digit_w), (int)digit_h);

    /* The additive 2 for the Arc diameter keeps the colon from looking too
       small at small display sizes. */

    XfFillArc(XtDisplay(w), w->dclock.colon[1], gc,
		(slope_add + digit_w) * 0.25 - 0.5, 0.75 * digit_h - 0.5,
		1 + seg_width/2, FADE_ITER);
    XfFillArc(XtDisplay(w), w->dclock.colon[1], gc,
		slope_add * 0.75 + digit_w * 0.25 - 0.5, 0.25 * digit_h - 0.5,
		1 + seg_width/2, FADE_ITER);
    XSetForeground(XtDisplay(w), gc, fadevector[FADE_ITER - 1]);

    /* to optimize scrolling information (see scroll_time()) */

    old_pix[0] = w->dclock.digits[0];
    old_pix[1] = old_pix[2] = old_pix[3] = 0;
    old_digs[0] = old_digs[1] = old_digs[2] = old_digs[3] = 0;

    if (w->dclock.date_fmt || !w->dclock.display_time || w->dclock.alarm ||
	w->dclock.bell)
	/* restore size */
	winheight = w->core.height;

    w->dclock.punt_resize = (XtIntervalId)NULL;
    Redisplay(w);
}

/* ARGSUSED */
static void
Resize  (w)
DclockWidget    w;
{
    /* Punt for 1/10 second on the resize, so that multiple events	*/
    /* created by an opaque resize do not overload the processor.	*/

    XtAppContext app;

    app = XtWidgetToApplicationContext((Widget)w);
    if (w->dclock.punt_resize != (XtIntervalId)NULL)
	XtRemoveTimeOut(w->dclock.punt_resize);
    w->dclock.punt_resize = XtAppAddTimeOut(app,
		(unsigned long)100, ResizeNow, (XtPointer)w);
}


static void
build_segments(wi, seg_pts, w, h)
DclockWidget wi;
segment_pts seg_pts;
float w, h;
{
    XfPoint *pts;
    float spacer, hskip, fslope, bslope, midpt, seg_width, segxw;
    float invcosphi, invsinphi, invcospsi, invsinpsi, slope;
    float dx1, dx2, dx3, dx4, dx5, dx6, dy1, dy2, dy5, dy6;
    float xfactor, temp_xpts[4];

    /* define various useful constants */
 
    segxw = sxw * w;
    slope = wi->dclock.angle;
    seg_width = wi->dclock.width_factor * w;
    spacer = (float)w * wi->dclock.space_factor;
    hskip = (float)seg_width * 0.125;
    fslope = 1 / (segxw/seg_width + 1/slope);
    bslope = -1 / (segxw/seg_width - 1/slope);
    midpt = h / 2;

    /* define some trigonometric values */
    /*  phi is the forward angle separating two segments; 
	psi is the reverse angle separating two segments. */

    invsinphi = sqrt(1 + fslope * fslope) / fslope;
    invcosphi = sqrt(1 + 1/(fslope * fslope)) * fslope;
    invsinpsi = sqrt(1 + bslope * bslope) / -bslope;
    invcospsi = sqrt(1 + 1/(bslope * bslope)) * bslope;

    /* define offsets from easily-calculated points for 6 situations */

    dx1 = hskip * invsinphi / (slope/fslope - 1);
    dy1 = hskip * invcosphi / (1 - fslope/slope);
    dx2 = hskip * invsinpsi / (1 - slope/bslope);
    dy2 = hskip * invcospsi / (bslope/slope - 1);
    dx3 = hskip * invsinphi;
    dx4 = hskip * invsinpsi;
    dx5 = hskip * invsinpsi / (1 - fslope/bslope);
    dy5 = hskip * invcospsi / (bslope/fslope - 1);
    dx6 = dy5;
    dy6 = dx5;

    /* calculate some simple reference points */

    temp_xpts[0] = spacer + (h - seg_width)/slope;
    temp_xpts[1] = spacer + (h - seg_width/2)/slope + segxw/2;
    temp_xpts[2] = spacer + h/slope + segxw;
    temp_xpts[3] = temp_xpts[0] + segxw;

    xfactor = w - 2 * spacer - h / slope - segxw;

    /* calculate the digit positions */

    pts = seg_pts[TOP];
    pts[0].y = pts[1].y = 0;
    pts[0].x = temp_xpts[2] - dx3;
    pts[1].x = w - spacer - segxw + dx4;
    pts[2].y = pts[5].y = (seg_width / 2) - dy5 - dy6;
    pts[5].x = temp_xpts[1] + dx5 - dx6;
    pts[2].x = pts[5].x + xfactor;
    pts[3].y = pts[4].y = seg_width;
    pts[4].x = temp_xpts[3] + dx4;
    pts[3].x = temp_xpts[0] + xfactor - dx3; 

    pts = &(seg_pts[MIDDLE][0]);
    pts[0].y = pts[1].y = midpt - seg_width/2;
    pts[0].x = spacer + (h - pts[0].y)/slope + segxw;
    pts[1].x = pts[0].x - segxw + xfactor;
    pts[2].y = pts[5].y = midpt;
    pts[3].y = pts[4].y = midpt + seg_width/2;
    pts[5].x = spacer + (h - pts[5].y)/slope + segxw/2;
    pts[2].x = pts[5].x + xfactor;
    pts[4].x = pts[0].x - seg_width/slope;
    pts[3].x = spacer + (h - pts[3].y)/slope + xfactor;

    pts = &(seg_pts[BOTTOM][0]);
    pts[3].y = pts[4].y = h;
    pts[2].y = pts[5].y = h - (seg_width / 2) + dy5 + dy6;
    pts[0].y = pts[1].y = h - seg_width;
    pts[0].x = spacer + segxw + seg_width/slope + dx3;  
    pts[1].x = spacer + (h - pts[1].y)/slope + xfactor - dx4;
    pts[4].x = spacer + segxw - dx4;
    pts[5].x = spacer + segxw/2 + (h - pts[5].y)/slope + dx6 - dx5;
    pts[2].x = pts[5].x + xfactor;
    pts[3].x = spacer + xfactor + dx3;

    pts = &(seg_pts[TOP_LEFT][0]);
    pts[0].y = seg_width / 2 - dy6 + dy5;
    pts[1].y = seg_width + dy2;
    pts[2].y = seg_pts[MIDDLE][0].y - 2 * dy1;
    pts[3].y = seg_pts[MIDDLE][5].y - 2 * dy6;
    pts[4].y = seg_pts[MIDDLE][0].y;
    pts[5].y = seg_width - dy1;
    pts[0].x = temp_xpts[1] - dx5 - dx6;
    pts[1].x = temp_xpts[3] - dx2;
    pts[2].x = seg_pts[MIDDLE][0].x + 2 * dx1; 
    pts[3].x = seg_pts[MIDDLE][5].x - 2 * dx6;
    pts[4].x = spacer + (h - pts[4].y)/slope;
    pts[5].x = temp_xpts[0] + dx1;

    pts = &(seg_pts[BOT_LEFT][0]);
    pts[0].y = seg_pts[MIDDLE][5].y + 2 * dy5;
    pts[1].y = seg_pts[MIDDLE][4].y + 2 * dy2;
    pts[2].y = seg_pts[BOTTOM][0].y - dy1;
    pts[3].y = seg_pts[BOTTOM][5].y - 2 * dy6;
    pts[4].y = h - seg_width + dy2;
    pts[5].y = midpt + seg_width/2;
    pts[0].x = seg_pts[MIDDLE][5].x - 2 * dx5;
    pts[1].x = seg_pts[MIDDLE][4].x - 2 * dx2;
    pts[2].x = seg_pts[BOTTOM][0].x - dx3 + dx1;
    pts[3].x = seg_pts[BOTTOM][5].x - 2 * dx6;
    pts[4].x = spacer + seg_width / slope - dx2;
    pts[5].x = spacer + (midpt - seg_width/2) / slope;

    pts = &(seg_pts[TOP_RIGHT][0]);
    pts[0].y = seg_width/2 - dy5 + dy6;
    pts[1].y = seg_width - dy2;
    pts[2].y = midpt - seg_width/2;
    pts[3].y = midpt - 2 * dy5;
    pts[4].y = pts[2].y - 2 * dy2;
    pts[5].y = seg_width + dy1;
    pts[0].x = temp_xpts[1] + xfactor + dx5 + dx6;
    pts[1].x = temp_xpts[3] + xfactor + dx1;
    pts[2].x = seg_pts[MIDDLE][0].x + xfactor;
    pts[3].x = seg_pts[MIDDLE][5].x + xfactor + dx5 * 2;
    pts[4].x = seg_pts[TOP_LEFT][4].x + xfactor + dx2 * 2;
    pts[5].x = temp_xpts[0] + xfactor - dx1;

    pts = &(seg_pts[BOT_RIGHT][0]);
    pts[0].y = seg_pts[MIDDLE][2].y + 2 * dy6;
    pts[1].y = midpt + seg_width / 2;
    pts[2].y = h - seg_width + dy1;
    pts[3].y = h - (seg_width / 2) + dy6 - dy5;
    pts[4].y = h - seg_width - dy2;
    pts[5].y = seg_pts[MIDDLE][3].y + 2 * dy1;
    pts[0].x = seg_pts[MIDDLE][2].x + 2 * dx6;
    pts[1].x = seg_pts[MIDDLE][3].x + segxw;
    pts[2].x = seg_pts[BOTTOM][1].x + dx4 + segxw - dx1;
    pts[3].x = seg_pts[BOTTOM][2].x + 2 * dx5;
    pts[4].x = seg_pts[BOTTOM][1].x + dx4 + dx2;
    pts[5].x = seg_pts[MIDDLE][3].x - 2 * dx1;

}

/*------------------------------------------------------------------------*/
/* find the squared length of a wire (or distance between two points in   */
/* user space).                                                           */
/*------------------------------------------------------------------------*/
     
float sqwirelen(userpt1, userpt2)
  XfPoint *userpt1, *userpt2;
{
  float xdist, ydist;

  xdist = userpt2->x - userpt1->x;
  ydist = userpt2->y - userpt1->y;
  return (xdist * xdist + ydist * ydist);
}

/* Point-to-line segment distance measure */

float find_mindist(x, y, seglist, num_segs)
  float x, y;
  XfPoint *seglist;
  int num_segs;
{
   int i;
   double fd, mind = MAXFLOAT;
   float a, b, c, d, frac;
   float protod;
   XfPoint *pt1, *pt2, testpt;
      
   testpt.x = x;
   testpt.y = y;
      
   for (i = 0; i < num_segs; i++) {
      pt1 = seglist + i;
      pt2 = seglist + ((i + 1) % num_segs);
      c = sqwirelen(pt1, pt2);
      a = sqwirelen(pt1, &testpt);
      b = sqwirelen(pt2, &testpt);
      frac = a - b;
      if (frac >= c) d = b;
      else if (-frac >= c) d = a;
      else {
         protod = c + a - b;
         d = a - ((protod * protod) / (c * 4));
      }
      if (d < 0)	/* due to roundoff error */
	 fd = 0;	
      else
	 fd = sqrt((double)d);
      if (fd < mind) mind = fd;
   }  
   return mind;
}     

/*------------------------*/
/* Simple insideness test */
/*------------------------*/

int test_insideness(x, y, seglist, num_segs)
  float x, y;
  XfPoint *seglist;
  int num_segs;
{
   int i, stval = 0;
   XfPoint *pt1, *pt2;
   float stdir;
   for (i = 0; i < num_segs; i++) {
      pt1 = seglist + i;
      pt2 = seglist + ((i + 1) % num_segs);
      stdir = (pt2->x - pt1->x) * (y - pt1->y)
             - (pt2->y - pt1->y) * (x - pt1->x);
      stval += sign(stdir);
   }
   return (abs(stval) == num_segs) ? 1 : 0;
}

/*------------------------------------------------------*/
/* Antialiasing circle-filling routine			*/
/*------------------------------------------------------*/

static void
XfFillArc(dpy, pix, gc, center_x, center_y, radius, fgi)
   Display *dpy;
   Drawable pix;
   GC gc;
   float center_x, center_y, radius;
   int fgi;
{
   int i, j, color;
   float minx, miny, maxx, maxy, dist, xdist, ydist, rsq;

   minx = center_x - radius;
   miny = center_y - radius;
   maxx = center_x + radius;
   maxy = center_y + radius;
   rsq = radius * radius;

   /* scan through pixel centers in region */

   for (j = (int)(miny - 1.0); j < (int)(maxy + 1.0); j++) {
      for (i = (int)(minx - 1.0); i < (int)(maxx + 1.0); i++) {
	 xdist = center_x - ((float)i + 0.5);
	 ydist = center_y - ((float)j + 0.5);
	 dist = radius - (float)sqrt(xdist * xdist + ydist * ydist);

	 if (dist > 0) {
	    if ((dist >= 1.0) && (fgi > 0)) color = fadevector[fgi - 1];
	    else color = fadevector[(int)(dist * fgi)];

	    XSetForeground(dpy, gc, color);
	    XDrawPoint(dpy, pix, gc, i, j);
	 }
      }
   }
}

/*------------------------------------------------------*/
/* Wrapper for XFillPolygon, using floats for points	*/
/* Antialiasing goes here, if you have a good fill	*/
/* algorithm for it. . . 				*/
/*------------------------------------------------------*/

static void
XfFillPolygon(dpy, pix, gc, float_pts, num_pts, fgi)
Display *dpy;
Drawable pix;
GC gc;
XfPoint *float_pts;
int num_pts, fgi;
{
   int i, j, color;
   float minx, miny, maxx, maxy, dist;
   Boolean inside;

   /* find boundaries */

   minx = miny = 100000.0;
   maxx = maxy = -100000.0;
   for (i = 0; i < num_pts; i++) {
      if (float_pts[i].x < minx) minx = float_pts[i].x;
      if (float_pts[i].x > maxx) maxx = float_pts[i].x;
      if (float_pts[i].y < miny) miny = float_pts[i].y;
      if (float_pts[i].y > maxy) maxy = float_pts[i].y;
   }

   /* scan through pixel centers in region */

   for (j = (int)(miny - 1.0); j < (int)(maxy + 1.0); j++) {
      inside = False;
      for (i = (int)(minx - 1.0); i < (int)(maxx + 1.0); i++) {
	 dist = find_mindist((float)i + 0.5, (float)j + 0.5,
		float_pts, num_pts); 
	 if (dist < 2.0)
	    inside = test_insideness((float)i + 0.5, (float)j + 0.5,
		float_pts, num_pts);
	 if (inside) {
	    if ((dist >= 1.0) && (fgi > 0)) color = fadevector[fgi - 1];
	    else {
	       color = fadevector[(int)(dist * fgi)];
	    }
	    XSetForeground(dpy, gc, color);
	    XDrawPoint(dpy, pix, gc, i, j);
	 }
      }
   }
}

/*------------------------------------------------------*/

static void
make_number(dw, pix, gc, n, seg_pts)
DclockWidget dw;
Pixmap pix;
GC gc;
int n;
segment_pts seg_pts;
{
   Display *dpy = XtDisplay(dw);
   int i;

   for (i = 0; i != NUM_SEGS; ++i) {
      if (segmask[n] & msk(i))
	 XfFillPolygon(dpy, pix, gc, seg_pts[i], num_segment_pts, FADE_ITER);
      else
	 XfFillPolygon(dpy, pix, gc, seg_pts[i], num_segment_pts, 0);
   }
   XSetForeground(dpy, gc, fadevector[FADE_ITER - 1]);
}

/*----------------------------------------------------------------------*/
/* Variation of "make_number" to handle the first digit of "civilian	*/
/* time", which may be the number "1" (2 segments only) or blank.	*/
/*----------------------------------------------------------------------*/

static void
make_number_one(dw, pix, gc, n, seg_pts)
DclockWidget dw;
Pixmap pix;
GC gc;
int n;
segment_pts seg_pts;
{
   Display *dpy = XtDisplay(dw);
   int color;

   color = (n == 1) ? FADE_ITER : 0;
   XfFillPolygon(dpy, pix, gc, seg_pts[TOP_RIGHT], num_segment_pts, color);
   XfFillPolygon(dpy, pix, gc, seg_pts[BOT_RIGHT], num_segment_pts, color);
   XSetForeground(dpy, gc, fadevector[FADE_ITER - 1]);
}

/*------------------------------------------------------*/
/*------------------------------------------------------*/

static void
make_fade_number(dw, pix, gc, on_msk, turn_on_msk, turn_off_msk, iter)
DclockWidget dw;
Pixmap pix;
GC gc;
int on_msk, turn_on_msk, turn_off_msk;
int iter;
{
    Display *dpy = XtDisplay(dw);
    Pixmap on_stipple = fade_stip[iter],
	   off_stipple = fade_stip[FADE_ITER - iter];
    int i;

    if (!use_stipple) XSetFillStyle(dpy, gc, FillSolid);

    for (i = 0; i != NUM_SEGS; ++i)
    {
	if (on_msk & msk(i))
	{
	    if (use_stipple)
	       XSetFillStyle(dpy, gc, FillSolid);
	    XfFillPolygon(dpy, pix, gc, norm_segment_pts[i],
			 num_segment_pts, FADE_ITER);
	}
	else if (turn_on_msk & msk(i))
	{
	    if (use_stipple) {
	       XSetStipple(dpy, gc, on_stipple);
	       XSetFillStyle(dpy, gc, FillOpaqueStippled);
	    }
	    XfFillPolygon(dpy, pix, gc, norm_segment_pts[i],
			 num_segment_pts, iter);
	}
	else if (turn_off_msk & msk(i))
	{
	    if (use_stipple) {
	       XSetStipple(dpy, gc, off_stipple);
	       XSetFillStyle(dpy, gc, FillOpaqueStippled);
	    }
	    XfFillPolygon(dpy, pix, gc, norm_segment_pts[i],
			 num_segment_pts, FADE_ITER - iter - 1);
	}
    }

    if (use_stipple) XSetFillStyle(dpy, gc, FillSolid);
    XSetForeground(dpy, gc, fadevector[FADE_ITER - 1]);
}

/*------------------------------------------------------*/
/*------------------------------------------------------*/

static void
make_fade_number_one(dw, pix, gc, on_msk, turn_on_msk, turn_off_msk, iter)
DclockWidget dw;
Pixmap pix;
GC gc;
int on_msk, turn_on_msk, turn_off_msk;
int iter;
{
    Display *dpy = XtDisplay(dw);
    Pixmap on_stipple = fade_stip[iter],
	   off_stipple = fade_stip[FADE_ITER - iter];
    int i;

    if (!use_stipple) XSetFillStyle(dpy, gc, FillSolid);

    for (i = TOP_RIGHT; i < NUM_SEGS; ++i)
    {
	if (on_msk & msk(i))
	{
	    if (use_stipple)
	       XSetFillStyle(dpy, gc, FillSolid);
	    XfFillPolygon(dpy, pix, gc, norm_segment_pts[i],
			 num_segment_pts, FADE_ITER);
	}
	else if (turn_on_msk & msk(i))
	{
	    if (use_stipple) {
	       XSetStipple(dpy, gc, on_stipple);
	       XSetFillStyle(dpy, gc, FillOpaqueStippled);
	    }
	    XfFillPolygon(dpy, pix, gc, norm_segment_pts[i],
			 num_segment_pts, iter);
	}
	else if (turn_off_msk & msk(i))
	{
	    if (use_stipple) {
	       XSetStipple(dpy, gc, off_stipple);
	       XSetFillStyle(dpy, gc, FillOpaqueStippled);
	    }
	    XfFillPolygon(dpy, pix, gc, norm_segment_pts[i],
			 num_segment_pts, FADE_ITER - iter - 1);
	}
    }

    if (use_stipple) XSetFillStyle(dpy, gc, FillSolid);
    XSetForeground(dpy, gc, fadevector[FADE_ITER - 1]);
}

/* Play the bell---sophisticated version, with ability to configure */
/* separate audio files for the alarm and hourly bell */

static void playbell(DclockWidget w, int alarmtype)
{
   /* Try using an audio process.  If that fails, ring the bell */
   int status = 1;

   if ((((alarmtype == 0) && (w->dclock.alarmfile != (String)NULL &&
		strcasecmp(w->dclock.alarmfile, "NULL"))) ||
		((alarmtype == 1) && (w->dclock.bellfile != (String)NULL &&
		strcasecmp(w->dclock.bellfile, "NULL")))) &&
		w->dclock.audioplay)
   {

      pid_t cpid, bell_proc;
      char *locargv[4];
      char *proot = strrchr(w->dclock.audioplay, '/');

      if (proot == NULL) proot = w->dclock.audioplay;
      else proot++;
      locargv[0] = proot;
      if (alarmtype == 0)
         locargv[1] = w->dclock.alarmfile;
      else
         locargv[1] = w->dclock.bellfile;
      locargv[2] = NULL;

      if ((cpid = fork()) < 0) {
	 fprintf(stderr, "dclock: Fork error.  Reverting to system bell.\n");
	 w->dclock.audioplay = (String)NULL;
      }
      else if (cpid == 0) {
         execv(w->dclock.audioplay, locargv);
	 fprintf(stderr, "dclock: Unable to exec \"%s\".\n", w->dclock.audioplay);
	 exit(-1);
      }
      else {
	 if (waitpid(cpid, &status, 0) != cpid) {
	    fprintf(stderr, "dclock: Waitpid error (%d)\n", errno);
	    exit(-1);
	 }
	 if (WEXITSTATUS(status) != 0) {
	    fprintf(stderr, "dclock: \"%s\" returned an error.  Reverting to"
			" system bell.\n", w->dclock.audioplay);
	    w->dclock.audioplay = (String)NULL;	
	 }
      }
   }

   if (WEXITSTATUS(status) != 0) {
      XBell(XtDisplay(w), 50);
      if (alarmtype == 0)
	 XBell(XtDisplay(w), 50);
   }
}

/* ARGSUSED */
static void
Redisplay  (w)
DclockWidget    w;
{
    XtAppContext app;
    Boolean save_scroll = w->dclock.scroll;
    Boolean save_fade = w->dclock.fade;
    long t;

    if (!XtIsRealized((Widget)w))
	return;

    if (w->dclock.punt_resize != (XtIntervalId)NULL) return;

    if (w->dclock.interval_id != (XtIntervalId)NULL) {
	XtRemoveTimeOut(w->dclock.interval_id);
	w->dclock.interval_id = (XtIntervalId)NULL;
    }
    XFillRectangle(XtDisplay(w), XtWindow(w), w->dclock.backGC,
	0, 0, w->core.width, w->core.height);
    before.tm_min = before.tm_hour = before.tm_wday = -1;
    old_pix[0] = w->dclock.digits[0];
    old_pix[1] = old_pix[2] = old_pix[3] = 0;
    old_digs[0] = old_digs[1] = old_digs[2] = old_digs[3] = 0;
    w->dclock.scroll = FALSE;
    w->dclock.fade = FALSE;
    (void) show_time(w);
    w->dclock.scroll = save_scroll;
    w->dclock.fade = save_fade;
    app = XtWidgetToApplicationContext((Widget)w);
    if (w->dclock.display_time)
	if (w->dclock.seconds || w->dclock.blink)
	    w->dclock.interval_id = XtAppAddTimeOut(app,
		(unsigned long)1000, timeout, (XtPointer)w);
	else {
	    t = time(0);
	    w->dclock.interval_id =
		XtAppAddTimeOut(app,
			(unsigned long)(60 - (t % 60)) * 1000, 
			timeout, (XtPointer)w);
	}
}

static void
draw_seconds(w, sec_val)
DclockWidget w;
int sec_val;
{
    int digitxpos, digitypos;
    float digit_w = w->dclock.digit_w;
    float digit_h = w->dclock.digit_h;
    Display *dpy = XtDisplay(w);
    Window win = XtWindow(w);
    GC gc = w->dclock.foreGC;

    XSetRegion(dpy, gc, clip_small);
    digitxpos = (int)(winwidth - (3 * digit_w + slope_add) *
	    w->dclock.small_ratio);
    digitypos = (int)(BORDER*y_ratio + (1.0 - w->dclock.small_ratio) * 
	    digit_h)+TopOffset;
    XSetClipOrigin(dpy, gc, digitxpos, digitypos);
    XCopyArea(dpy, w->dclock.tiny_digits[sec_val/100], win, gc,
	    0, 0, (int)((digit_w + slope_add) * w->dclock.small_ratio), 
	    (int)(digit_h * w->dclock.small_ratio), digitxpos, digitypos);
    digitxpos += (int)(digit_w * w->dclock.small_ratio);
    XSetClipOrigin(dpy, gc, digitxpos, digitypos);
    XCopyArea(dpy, w->dclock.tiny_digits[(sec_val%100)/10], win, gc,
	    0, 0, (int)((digit_w + slope_add) * w->dclock.small_ratio), 
	    (int)(digit_h * w->dclock.small_ratio), digitxpos, digitypos);
    digitxpos += (int)(digit_w * w->dclock.small_ratio);
    XSetClipOrigin(dpy, gc, digitxpos, digitypos);
    XCopyArea(dpy, w->dclock.tiny_digits[(sec_val%100)%10], win, gc,
	    0, 0, (int)((digit_w + slope_add) * w->dclock.small_ratio), 
	    (int)(digit_h * w->dclock.small_ratio), digitxpos, digitypos);
}

static Boolean
show_time(w)
DclockWidget w;
{
    char buf[11];
    Boolean alarm_went_off = False;
    long t = time(0);
    register struct tm *l_time = localtime(&t);
    float digit_w = w->dclock.digit_w;
    float digit_h = w->dclock.digit_h;
    int digitxpos, digitypos;
    Display *dpy = XtDisplay(w);
    Window win = XtWindow(w);
    GC gc = w->dclock.foreGC;
    struct timeval now;

    if (w->dclock.utc == True) 
	l_time = gmtime(&t);

    maketimeofday(&now, NULL);
    l_time->tm_hour = (now.tv_sec/60)%60;
    l_time->tm_min = now.tv_sec%60;
    l_time->tm_sec = now.tv_usec/1000;
    if (w->dclock.display_time == True) {
	    (void) sprintf(buf, "%02d%02d", l_time->tm_hour, l_time->tm_min);
    } else
	/* display the alarm time */
	(void) sprintf(buf, "%02d%02d", Alarm.hrs, Alarm.mins);

    /* Copy the pre-defined pixmaps onto the viewing area.*/
    /* Start with the colon. */

    XSetRegion(dpy, gc, clip_colon);
    digitxpos = cx;
    digitypos = (int)(BORDER*y_ratio)+TopOffset;
    XSetClipOrigin (dpy, gc, digitxpos, digitypos);
    XCopyArea(dpy,
	w->dclock.colon[!w->dclock.blink || l_time->tm_min & 1],
	win, gc, 0, 0, (int)(slope_add + 0.5 * digit_w), (int)digit_h,
	digitxpos, digitypos);

    //gettimeofday(&now, NULL);
    //maketimeofday(&now, NULL);
    //l_time->tm_sec = now.tv_usec/10000;
    /* Next the seconds. */
    if (w->dclock.seconds)
	draw_seconds(w, l_time->tm_sec);

    /* The large digits are displayed by the scrolling/fading routine. */
    if (l_time->tm_sec != before.tm_sec || l_time->tm_min != before.tm_min)
	scroll_time(w, buf);

    XSetClipMask(dpy,gc,None);

    if ((w->dclock.date_fmt && before.tm_wday != l_time->tm_wday) ||
		(!w->dclock.display_time))
	show_date(w, l_time);
    if (w->dclock.alarm) show_alarm(w);
    if (w->dclock.bell) show_bell(w);

    before = *l_time;

    if (w->dclock.alarm && Alarm.hrs == l_time->tm_hour &&
	Alarm.mins == l_time->tm_min &&
	l_time->tm_sec < 5) {
	bsave |= 0x4;		/* set alarm_active state */
	playbell(w, 0);
	toggle_reverse_video(w);
	alarm_went_off = True;
    } else {
	/* if alarm didn't go off, check for hour/half-hour bell */
	if (w->dclock.bell && (!w->dclock.seconds || l_time->tm_sec == 0) &&
	    (l_time->tm_min == 0 || l_time->tm_min == 30)) {
	    playbell(w, 1);
	    if (l_time->tm_min == 0)
		playbell(w, 1);
	}

	/* avoid leaving clock in reverse_video state after alarm, unless */
	/* alarmPersist is set, in which case we always leave dclock in	  */
	/* reverse video after an alarm.				  */

	if (bsave & 0x4) {
	    if (((bsave & 0x1) == ((bsave & 0x2) >> 1)) ^
			((w->dclock.alarm_persist) ? 0 : 1))
		toggle_reverse_video(w);

	    bsave &= 0x3;	/* clear alarm_active state */
	}
    }
    return alarm_went_off;
}


static void
scroll_time(w, p)
DclockWidget w;
register char *p;
{
    int chgd[4], J = winheight - BORDER*2*y_ratio + 1;
    register int i, j, incr;
    float digit_w = w->dclock.digit_w;
    float digit_h = w->dclock.digit_h;
    Display *dpy = XtDisplay(w);
    Window win = XtWindow(w);
    GC gc = w->dclock.foreGC;
    Pixmap new_pix[4];
    int new_digs[4], digitxpos, digitypos;
    int cur_sec = 0;
    long t;
    register struct tm *now;

/* definitions for the window x and y positions of each of the large digits. */

#define x (int)((i>1)*(digit_w / 2) + digit_w * i - (!w->dclock.miltime) \
	*((1 - 2 * w->dclock.space_factor) * digit_w - segxwidth))
#define y (int)((BORDER * y_ratio)+TopOffset)

    for (i = 0; i < 4; i++)
	new_digs[i] = *p++ - '0';

    if (w->dclock.miltime)
	new_pix[0] = w->dclock.digits[new_digs[0]];
    else
	new_pix[0] = w->dclock.digit_one[new_digs[0]];

    for (i = 1; i < 4; i++)
	new_pix[i] = w->dclock.digits[new_digs[i]];

    digitypos = (int)(BORDER*y_ratio)+TopOffset;

    /* digit scrolling routine. */

    if (w->dclock.scroll)
    {
	for (i = 0; i < 4; i++)    /* if pixmaps don't match, scroll it */
	    chgd[i] = (new_pix[i] != old_pix[i]);

	if (w->dclock.date_fmt || !w->dclock.display_time)
	    J -= w->dclock.font->ascent + w->dclock.font->descent;

	if ((incr = J / 30) < 1)
	    incr = 1;

        XSetRegion(dpy, gc, clip_norm);
	for (j = 0; j <= J; j += incr)
	    for (i = 0; i < 4; i++)
		if (chgd[i]) {
		    digitxpos = x;
		    XSetClipOrigin(dpy, gc, digitxpos, digitypos);
		    if (old_pix[i])
			XCopyArea(dpy, old_pix[i], win, gc,
			    0, j, (int)(digit_w + slope_add), 
			    (int)(digit_h) - j, digitxpos, y);

		    XCopyArea(dpy, new_pix[i], win, gc,
			    0, 0, (int)(digit_w + slope_add), j, 
			    digitxpos, y + (int)digit_h - j);
		}
	XSetClipMask(dpy, gc, None);
    }

    /* digit fading routine */

    else if (w->dclock.fade)
    {
	Pixmap tmp_pix[4];
	int oldmask, newmask;
	int stay_on[4], turn_on[4], turn_off[4];
	unsigned long fade_rate = w->dclock.fade_rate * 1000;

	for (i = 0; i < 4; i++)    /* if pixmaps don't match, fade it */
	    if (chgd[i] = (new_pix[i] != old_pix[i]))
	    {
		tmp_pix[i] = XCreatePixmap(dpy, win, (int)(digit_w + slope_add),
			(int)digit_h, DefaultDepthOfScreen(XtScreen(w)));
		oldmask = segmask[old_digs[i]];
		newmask = segmask[new_digs[i]];
		stay_on[i] = oldmask & newmask;
		turn_on[i] = ~oldmask & newmask;
		turn_off[i] = oldmask & ~newmask;
	    }
	    else
		tmp_pix[i] = (Pixmap)NULL;
  
	XSetClipMask(dpy, gc, None);
	for (j = 1; j != FADE_ITER; ++j)
	{
	    for (i = 0; i < 4; i++)
		if (chgd[i])
		{
		    XFillRectangle(dpy, tmp_pix[i], w->dclock.backGC,
			0, 0, (int)(digit_w + slope_add), (int)digit_h);
		    if (i == 0 && !w->dclock.miltime)
			make_fade_number_one(w, tmp_pix[i], gc, stay_on[i],
				turn_on[i], turn_off[i], j);
		    else
			make_fade_number(w, tmp_pix[i], gc, stay_on[i],
				turn_on[i], turn_off[i], j);
		    digitxpos = x;
		    XSetRegion(dpy, gc, clip_norm);
		    XSetClipOrigin(dpy, gc, digitxpos, digitypos);
		    XCopyArea(dpy, tmp_pix[i], win, gc, 0, 0, (int)(digit_w + 
		        slope_add), (int)digit_h, digitxpos, y);
		    XSetClipMask(dpy, gc, None);
		}

	    XFlush(dpy);
	    usleep(fade_rate);

	    /* Keep tabs on time;  it's necessary to update the seconds */
	    /* if fade_rate * FADE_ITER + (overhead) > 1 second 	*/

	    t = time(0);
	    now = localtime(&t);
            if (w->dclock.seconds && (now->tm_sec != cur_sec)) {
	       cur_sec = now->tm_sec;
	       draw_seconds(w, cur_sec);
    	       XSetClipMask(dpy,gc,None);
	    }
	} 

	for (i = 0; i < 4; ++i)
	    if (tmp_pix[i]) XFreePixmap(dpy, tmp_pix[i]);
    }

    XSetRegion(dpy, gc, clip_norm);
    for (i = 0; i < 4; i++) {
	digitxpos = x;
	XSetClipOrigin(dpy, gc, digitxpos, digitypos);
	XCopyArea(dpy, new_pix[i], win, gc, 0,0, (int)(digit_w + 
		slope_add), (int)digit_h, digitxpos, y);
    }
    XSetClipMask(dpy, gc, None);
    if (!w->dclock.display_time) outline_digit(w, cur_position, True);

    for (i = 0; i < 4; i++) {
	old_pix[i] = new_pix[i];
	old_digs[i] = new_digs[i];
    }

#undef x
#undef y

}

static void
show_date(w, now)
DclockWidget w;
struct tm *now;
{
    Display *dpy = XtDisplay(w);
    Window win = XtWindow(w);
    char datestr[128];
    register char *p;
    int x, datep;
#ifdef XFT_SUPPORT
    int tsize = w->core.height / 6;
    XGlyphInfo xftextents;
#else
    int tsize = w->dclock.font->ascent + w->dclock.font->descent;
#endif

    if (!w->dclock.display_time)
	datep = strlen(strcpy(datestr, "Push HERE to Set/Unset Alarm"));
    else
        datep = strftime(datestr, 128, w->dclock.date_fmt, now);

#ifdef XFT_SUPPORT
    XftTextExtents8(dpy, w->dclock.xftfont, datestr, datep, &xftextents);
    x = (w->core.width - xftextents.width) / 2;
#else
    x = (w->core.width - XTextWidth(w->dclock.font, datestr, datep)) / 2;
#endif
    if (x < 2)
	x = 2;
    if (TopOffset) {

#ifdef XFT_SUPPORT
	XftDrawRect(w->dclock.xftdraw, &w->dclock.xftbg, 0, 0, winwidth, tsize);
	XftDrawString8(w->dclock.xftdraw, &w->dclock.xftfg, w->dclock.xftfont, x,
		(BORDER/2) + tsize - 2, datestr, datep);
#else
	XFillRectangle(dpy, win, w->dclock.backGC,
	    0, 0, winwidth, tsize);
	XDrawString(dpy, win, w->dclock.foreGC,
		x, ((BORDER/2)+w->dclock.xftfont->height), datestr, datep);
#endif
    } else {

#ifdef XFT_SUPPORT
	XftDrawRect(w->dclock.xftdraw, &w->dclock.xftbg, 0, winheight - tsize,
		winwidth, tsize);
	XftDrawString8(w->dclock.xftdraw, &w->dclock.xftfg, w->dclock.xftfont, x,
		winheight - BORDER - 2, datestr, datep);
#else
	XFillRectangle(dpy, win, w->dclock.backGC,
	    0, winheight - tsize, winwidth, tsize);
	XDrawString(dpy, win, w->dclock.foreGC,
		x, winheight - BORDER, datestr, datep);
#endif
    }
}

static void
show_alarm(w)
DclockWidget w;
{
    Display *dpy = XtDisplay(w);
    Window win = XtWindow(w);
    int tsize = w->dclock.font->ascent + w->dclock.font->descent;
    int ccentx = BORDER;
    int hcsize = tsize >> 1;
    int ccenty;

    if (TopOffset)
	ccenty = BORDER + (hcsize/2);
    else
	ccenty = winheight - BORDER - hcsize;

    hcsize /= 2;
    XDrawArc(dpy, win, w->dclock.foreGC, ccentx - hcsize,
		ccenty - hcsize, hcsize << 1, hcsize << 1, -2800, 5600);
    hcsize *= 2;
    XDrawArc(dpy, win, w->dclock.foreGC, ccentx - hcsize,
		ccenty - hcsize, hcsize << 1, hcsize << 1, -2800, 5600);
    hcsize *= 1.5;
    XDrawArc(dpy, win, w->dclock.foreGC, ccentx - hcsize,
		ccenty - hcsize, hcsize << 1, hcsize << 1, -2800, 5600);
}

static void
show_bell(w)
DclockWidget w;
{
    Display *dpy = XtDisplay(w);
    Window win = XtWindow(w);
    int tsize = w->dclock.font->ascent + w->dclock.font->descent;
    int hcsize = tsize >> 1;
    int ccentx = BORDER + ((w->dclock.alarm) ? (tsize * 3 / 2) : (tsize / 3));
    int ccenty;

    if (TopOffset)
	ccenty = BORDER + (hcsize/2);
    else
	ccenty = winheight - BORDER - hcsize;

    XDrawArc(dpy, win, w->dclock.foreGC, ccentx - tsize / 3,
	ccenty - tsize / 2, tsize * 2 / 3, tsize * 2 / 3, 0, 11520);

    XDrawLine(dpy, win, w->dclock.foreGC, ccentx - tsize / 3,
	ccenty - tsize / 6, ccentx - tsize / 3, ccenty + tsize / 3);
    XDrawLine(dpy, win, w->dclock.foreGC, ccentx + tsize / 3,
	ccenty - tsize / 6, ccentx + tsize / 3, ccenty + tsize / 3);

    XDrawArc(dpy, win, w->dclock.foreGC, ccentx - tsize * 2 / 3,
	ccenty + tsize / 6, tsize / 3, tsize / 3, 17280, 5760);
    XDrawArc(dpy, win, w->dclock.foreGC, ccentx + tsize / 3,
	ccenty + tsize / 6, tsize / 3, tsize / 3, 11520, 5760);

    XDrawLine(dpy, win, w->dclock.foreGC, ccentx - tsize / 2,
	ccenty + tsize / 2, ccentx + tsize / 2, ccenty + tsize / 2);

    XDrawArc(dpy, win, w->dclock.foreGC, ccentx - tsize / 12,
	ccenty + tsize / 2, tsize / 6, tsize / 6, 0, 23040);
}


static void
timeout(w, id)
DclockWidget w;
XtIntervalId *id;
{
    struct timeval now;
    int next_intr;
    Boolean alarm_went_off = show_time(w);
    XtAppContext app;

    /* Try to catch the next second on the second boundary.	*/  

    /* NO!  This is a BAD IDEA, and leads to catching the 	*/
    /* time sometimes before the second, sometimes after, and	*/
    /* thereby skipping seconds all the time.  Instead, we	*/
    /* should aim for a few milliseconds LATE (try 5ms).	*/

    //gettimeofday(&now, NULL);
    maketimeofday(&now, NULL);
    /* next_intr = (1e6 - now.tv_usec) / 1e3; */
    next_intr = (1e6 - now.tv_usec + 5000) / 1e3;

    app = XtWidgetToApplicationContext((Widget)w);
    w->dclock.interval_id =
	XtAppAddTimeOut(app, 5, timeout,
		(XtPointer)w);
}

/* ARGSUSED */
static Boolean
SetValues (current, request, new)
DclockWidget current, request, new;
{
    Boolean do_redraw = False;

    if (current->dclock.display_time != new->dclock.display_time) {
	before.tm_min = before.tm_hour = before.tm_wday = -1;
	new->dclock.miltime = True; /* old state needs to be saved */
	do_redraw = True;
    }
    if (current->dclock.alarm_time != new->dclock.alarm_time) {
	if (sscanf(new->dclock.alarm_time, "%2d:%2d",
		&Alarm.hrs, &Alarm.mins) != 2 || Alarm.hrs >= 24 ||
		Alarm.mins >= 60) {
	    XtWarning("Alarm Time is in incorrect format.");
	    new->dclock.alarm_time = "00:00";
	    Alarm.hrs = Alarm.mins = 0;
	}
	new->dclock.alarm_time =
	    strcpy(XtMalloc(strlen(new->dclock.alarm_time)+1),
				   new->dclock.alarm_time);
	do_redraw = True;
    }

    if (new->dclock.foreground != current->dclock.foreground
    ||  new->dclock.background != current->dclock.background
    ||  new->dclock.tails != current->dclock.tails
    ||  new->dclock.fade != current->dclock.fade
    ||  new->dclock.miltime != current->dclock.miltime) {
	XtReleaseGC ((Widget)current, current->dclock.foreGC);
	XtReleaseGC ((Widget)current, current->dclock.backGC);
	GetGC(new);
	ResizeNow(new); /* pixmaps need to be redrawn */
	do_redraw = True;
    }
    if (new->dclock.seconds != current->dclock.seconds) {
	if (current->dclock.interval_id != (XtIntervalId)NULL) {
	    XtRemoveTimeOut(current->dclock.interval_id);
	    current->dclock.interval_id = (XtIntervalId)NULL;
	}
	ResizeNow(new);
	do_redraw = True;
    }
    if (new->dclock.date_fmt != current->dclock.date_fmt) {
	do_redraw = True;
	before.tm_wday = -1;
    }
    if (new->dclock.alarm != current->dclock.alarm)
	do_redraw = True;

    return do_redraw;
}

/* The following routines are called by the various keypress events. */

static void
decrease_space(w)
DclockWidget w;
{
    w->dclock.space_factor *= 0.95;
    if (w->dclock.space_factor < 0.0) w->dclock.space_factor = 0;
    ResizeNow(w);
    Redisplay(w);
}

static void
increase_space(w)
DclockWidget w;
{
    w->dclock.space_factor *= 1.05;
    if (w->dclock.space_factor > 0.25) w->dclock.space_factor = 0.25;
    ResizeNow(w);
    Redisplay(w);
}

static void
decrease_slope(w)
DclockWidget w;
{
    w->dclock.angle *= 1.05;
    if (w->dclock.angle > 2 * winheight) w->dclock.angle = 2 * winheight;
    ResizeNow(w);
    Redisplay(w);
}

static void
increase_slope(w)
DclockWidget w;
{
    w->dclock.angle *= 0.95;
    if (w->dclock.angle < 1.0) w->dclock.angle = 1.0;
    ResizeNow(w);
    Redisplay(w);
}

static void
widen_segment(w)
DclockWidget w;
{
    w->dclock.width_factor *= 1.05;
    if (w->dclock.width_factor > 0.4) w->dclock.width_factor = 0.4;
    ResizeNow(w);
    Redisplay(w);
}

static void
thin_segment(w)
DclockWidget w;
{
    w->dclock.width_factor *= 0.95;
    if (w->dclock.width_factor < 0.01) w->dclock.width_factor = 0.01;
    ResizeNow(w);
    Redisplay(w);    
}

static void
print_help(w)
DclockWidget w;
{
fprintf(stderr, "In-Window single-character commands:\n\n");
fprintf(stderr, "   r	Toggles Reverse Video.\n");
fprintf(stderr, "   s	Toggles the seconds display.\n");
fprintf(stderr, "   b	Toggles the bell attribute.\n");
fprintf(stderr, "   j	Toggles the jump/scroll attribute.\n");
fprintf(stderr, "   f	Toggles the fade attribute.\n");
fprintf(stderr, "   d	Toggles the date format.\n");
fprintf(stderr, "   u	Toggles the location of the date (top/bottom).\n");
fprintf(stderr, "   m	Toggles the military time format.\n");
fprintf(stderr, "   a	Toggles the alarm clock.\n");
fprintf(stderr, "   t	Toggles the tails attribute.\n");
fprintf(stderr, "   v	Ascii dump of the values of various attributes.\n");
fprintf(stderr, "   h	Print this help screen.\n");
fprintf(stderr, "   ?	Same as h.\n");
fprintf(stderr, "   :	Toggles the blinking colon.\n");
fprintf(stderr, "   /	Increases the slope of the digits.\n");
fprintf(stderr, "   \\	Decreases the slope of the digits.\n");
fprintf(stderr, "   +	Increases the thickness of the digit segments.\n");
fprintf(stderr, "   -	Decreases the thickness of the digit segments.\n");
fprintf(stderr, "   >	Increases spacing between the digits.\n");
fprintf(stderr, "   <	Decreases spacing between the digits.\n");
fprintf(stderr, "   q	quit the program.\n");
fprintf(stderr, " <Btn3>	Toggle Alarm-time display\n");
fprintf(stderr, " <Btn1>	Increment number in alarm-time display\n");
fprintf(stderr, " <Btn2>	Decrement number in alarm-time display\n");
fprintf(stderr, "\n\n");
}

static void
print_dump(w)
DclockWidget w;
{
fprintf(stderr, "Window attributes:\n\n");
fprintf(stderr, "   window width = %d\n", winwidth);
fprintf(stderr, "   window height = %d\n", winheight);
fprintf(stderr, "   slope = %f\n", w->dclock.angle);
fprintf(stderr, "   segment width ratio = %f\n", w->dclock.width_factor);
fprintf(stderr, "   seconds gap ratio = %f\n", w->dclock.sec_gap);
fprintf(stderr, "   small digit ratio = %f\n", w->dclock.small_ratio);
fprintf(stderr, "   digit spacing ratio = %f\n", w->dclock.space_factor);
fprintf(stderr, "   fade rate = %d\n", w->dclock.fade_rate);
fprintf(stderr, "   date format = %s\n", (w->dclock.date_fmt) ?
					 (w->dclock.date_fmt) : "<null>");
fprintf(stderr, "   date location = %s\n", (w->dclock.dateup ? "top" : "bottom"));
fprintf(stderr, "\n\n");
}

static void
toggle_date(w)
DclockWidget w;
{
    char *tmp;

    if (!w->dclock.display_time) {
	XBell(XtDisplay(w), 50);
	return;
    }
    tmp = w->dclock.date_fmt;
    w->dclock.date_fmt = saved_date;
    saved_date = tmp;

    if (w->dclock.dateup && w->dclock.date_fmt)
#ifdef XFT_SUPPORT
	TopOffset = w->core.height / 6;
#else
	TopOffset = w->dclock.font->ascent + w->dclock.font->descent;
#endif
    else
	TopOffset = 0;

    before.tm_wday = -1;
    ResizeNow(w);
    Redisplay(w);
}

static void
toggle_dateup(w)
DclockWidget w;
{
    Arg arg;
    int tmp;

    if (!w->dclock.display_time) {
	XBell(XtDisplay(w), 50);
	return;
    }

    XtSetArg(arg, XtNdateUp, !w->dclock.dateup);
    XtSetValues((Widget)w, &arg, 1);

    if (w->dclock.dateup && w->dclock.date_fmt)
#ifdef XFT_SUPPORT
	TopOffset = w->core.height / 6;
#else
	TopOffset = w->dclock.font->ascent + w->dclock.font->descent;
#endif
    else
	TopOffset = 0;

    ResizeNow(w);
    Redisplay(w);
}

static void
toggle_bell(w)
DclockWidget w;
{
    if (w->dclock.bell = !w->dclock.bell) {
	playbell(w, 1);
    }
    ResizeNow(w);
    Redisplay(w);
}

static void
toggle_scroll(w)
DclockWidget w;
{
    w->dclock.scroll = !w->dclock.scroll;
}

static void
toggle_reverse_video(w)
DclockWidget w;
{
   Display *dpy = XtDisplay(w);
   int i, itemp;

   if (!(bsave & 0x1)) {
      XSetForeground(dpy, w->dclock.backGC, w->dclock.foreground);
      bsave |= 0x1;	/* set reverse-video state */
   }
   else {
      XSetForeground(dpy, w->dclock.backGC, w->dclock.background);
      bsave &= 0x5;	/* clear reverse-video state */
   }

   if (!(bsave & 0x4))
	bsave = (bsave & 0x1) ? 0x3 : 0x0;

   for (i = 0; i < (FADE_ITER >> 1); i++) {
      itemp = fadevector[i];
      fadevector[i] = fadevector[FADE_ITER - i - 1];
      fadevector[FADE_ITER - i - 1] = itemp;
   }
   ResizeNow(w);
   Redisplay(w);
}

static void
toggle_military_time(w)
DclockWidget w;
{
    Arg arg;

    if (!w->dclock.display_time) {
	XBell(XtDisplay(w), 50);
	return;
    }
    XtSetArg(arg, XtNmilitaryTime, !w->dclock.miltime);
    XtSetValues((Widget)w, &arg, 1);
}

static void
toggle_blink(w)
DclockWidget w;
{
    Arg arg;

    if (!w->dclock.display_time) {
	XBell(XtDisplay(w), 50);
	return;
    }
    XtSetArg(arg, XtNblink, !w->dclock.blink);
    XtSetValues((Widget)w, &arg, 1);
}

static void
toggle_seconds(w)
DclockWidget w;
{
    Arg arg;

    if (!w->dclock.display_time) {
	XBell(XtDisplay(w), 50);
	return;
    }
    XtSetArg(arg, XtNseconds, !w->dclock.seconds);
    XtSetValues((Widget)w, &arg, 1);
}

static void
toggle_fade(w)
DclockWidget w;
{
    Arg arg;

    XtSetArg(arg, XtNfade, !w->dclock.fade);
    XtSetValues((Widget)w, &arg, 1);
    if (w->dclock.fade && w->dclock.scroll)
	toggle_scroll(w);
}

static void
toggle_tails(w)
DclockWidget w;
{
    Arg arg;

    XtSetArg(arg, XtNtails, !w->dclock.tails);
    XtSetValues((Widget)w, &arg, 1);
}

static void
toggle_alarm(w)
DclockWidget w;
{
    Arg arg;

    XtSetArg(arg, XtNalarm, !w->dclock.alarm);
    XtSetValues((Widget)w, &arg, 1);
}

static void
set_alarm(w, event)
DclockWidget w;
XButtonEvent *event;
{
    static saved_secs, saved_miltime, saved_fade, saved_blink;

    if (event->button == 3) {
	if (!(w->dclock.display_time = !w->dclock.display_time)) {
	    XtRemoveTimeOut(w->dclock.interval_id);
	    saved_secs = w->dclock.seconds, w->dclock.seconds = False;
	    saved_miltime = w->dclock.miltime, w->dclock.miltime = True;
	    saved_fade = w->dclock.fade, w->dclock.fade = False;
	    saved_blink = w->dclock.blink, w->dclock.blink = False;
	    w->dclock.interval_id = (XtIntervalId)NULL;
	} else {
	    w->dclock.seconds = saved_secs;
	    w->dclock.miltime = saved_miltime;
	    w->dclock.fade = saved_fade;
	    w->dclock.blink = saved_blink;
	}
	ResizeNow(w);
	Redisplay(w);
    } else if (!w->dclock.display_time &&
	    (event->button == 1 || event->button == 2)) {
	/* get the digit under the position (1-4) the mouse is over
	 * and increment (possibly wrap around) to next digit.
	 */
	int i, x, y = (int)((BORDER/2)*y_ratio) + TopOffset;
	/* first check to see if user toggles the alarm */
#ifdef XFT_SUPPORT
	if (TopOffset)
		i = (BORDER + (w->core.height / 6))
			- event->y;
	else
		i = event->y - (winheight - (w->core.height / 6));
#else
	if (TopOffset)
		i = (BORDER + (w->dclock.font->ascent + w->dclock.font->descent))
			- event->y;
	else
		i = event->y - (winheight - (w->dclock.font->ascent
			+ w->dclock.font->descent));
#endif
	if (i >= 0)
	    toggle_alarm(w);
	else for (i = 0; i < 4; i++) {
	    x = i * w->dclock.digit_w + (i>1) * (w->dclock.digit_w / 2) + 
		(w->dclock.digit_h - event->y) / w->dclock.angle;
	    if (event->x < x + w->dclock.digit_w) {
		if (cur_position == i) {
		    int digit = w->dclock.alarm_time[i>1?i+1:i] - '0';
		    int mod;
		    switch (i) {
			case 0:
			    if (Alarm.hrs > 13 && digit == 1)
				digit++;
			    mod = 3;
			    before.tm_hour = -1;
			when 1 :
			    mod = (Alarm.hrs < 20)? 10 : 4;
			    before.tm_hour = -1;
			when 2:
			    mod = 6;
			    before.tm_min = -1;
			when 3 :
			    mod = 10;
			    before.tm_min = -1;
		    }
		    if (event->button == 1)
			digit = (digit + 1) % mod;
		    else if (--digit < 0)
			digit = mod-1;
		    w->dclock.alarm_time[i>1?i+1:i] = digit + '0';
		    (void) sscanf(w->dclock.alarm_time, "%2d:%2d",
			    &Alarm.hrs, &Alarm.mins);
		    (void) show_time(w);
		    outline_digit(w, cur_position, True);
		} else {
		outline_digit(w, cur_position, False);
		outline_digit(w, cur_position = i, True);
		}
		break;
	    }
	}
    } else
	XBell(XtDisplay(w), 50);
}

/* Draw a box around the digit, for use with the alarm-time setting function.*/

static void
outline_digit(w, i, draw_it)
DclockWidget w;
int i;
Boolean draw_it;
{
    int xbase, y, xadd;
    XPoint rectpts[5];

    xbase = w->dclock.digit_w * i + (i>1) * (w->dclock.digit_w / 2);
    xadd = (int)(w->dclock.space_factor * w->dclock.digit_w / 4);
    y = 0.5 * BORDER * y_ratio;

    rectpts[0].x = rectpts[4].x = xbase + xadd;
    rectpts[1].x = rectpts[0].x + ((w->dclock.digit_h + 2 * y) /
	w->dclock.angle);
    rectpts[2].x = rectpts[1].x + w->dclock.digit_w - xadd;
    rectpts[3].x = rectpts[0].x + w->dclock.digit_w - xadd;
    rectpts[0].y = rectpts[4].y = rectpts[3].y = (w->dclock.digit_h + 3 * y) + TopOffset;
    rectpts[1].y = rectpts[2].y = y + TopOffset; 

    XDrawLines(XtDisplay(w), XtWindow(w),
	draw_it? w->dclock.foreGC : w->dclock.backGC,
	rectpts, 5, CoordModeOrigin);
}
