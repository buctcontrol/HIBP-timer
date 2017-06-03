/*
 * Dclock.c -- a digital clock widget.
 * Copyright (c) 1988 Dan Heller <argv@sun.com>
 */
#ifndef _XtDclock_h
#define _XtDclock_h

/* Parameters:

 Name                Class              RepType         Default Value
 ----                -----              -------         -------------
 alarm		     Boolean		Boolean		False
 alarmTime	     Time		long		00:00:00
 alarmPersist	     Boolean		Boolean		False
 displayTime	     Boolean		Boolean		True
 bell		     Boolean		Boolean		False
 border              BorderColor        pixel           Black      
 borderWidth         BorderWidth        int             1
 date		     String		String		NULL
 dateUp		     Boolean		Boolean		False
 destroyCallback     Callback           Pointer         NULL
 foreground          Foreground         Pixel           Chartreuse
 led_off             Foreground         Pixel           ForestGreen
 background          Background         Pixel           DarkSlateGray

 height              Height             int             80
 mappedWhenManaged   MappedWhenManaged  Boolean         True
 seconds	     Boolean		Boolean		False
 blink		     Boolean		Boolean		True
 scroll		     Boolean		Boolean		False
 fade		     Boolean		Boolean		True
 fadeRate	     Time		int		50
 tails		     Boolean		Boolean		True
 time		     Time		long		current time
 x                   Position           int             0 
 y                   Position           int             0

 fontName	     String		String		"charter:italic"

*/

#ifndef XtRDimension
#define XtRDimension	"dimension"
#endif /* XtRDimension */

#define XtRTime		"Time"
#define XtCTime		"Time"

#define XtNled_off	"led_off"
#define XtNseconds	"seconds"
#define XtNalarm	"alarm"
#define XtNalarmTime	"alarmTime"
#define XtNdisplayTime	"displayTime"
#define XtNtime		"time"
#define XtNbell		"bell"
#define XtNscroll	"scroll"
#define XtNfade		"fade"
#define XtNfadeRate	"fadeRate"
#define XtNtails	"tails"
#define XtNdate		"date"
#define XtNdateUp	"dateUp"
#define XtNmilitaryTime	"militaryTime"
#define XtNutcTime	"utcTime"
#define XtNangle	"angle"
#define XtNwidthFactor  "widthFactor"
#define XtNsmallRatio	"smallRatio"
#define XtNsecondGap	"secondGap"
#define XtNspaceFactor  "spaceFactor"
#define XtNblink	"blink"
#define XtNalarmFile	"alarmFile"
#define XtNbellFile	"bellFile"
#define XtNaudioPlay	"audioPlay"
#define XtNalarmPersist "alarmPersist"

#ifdef XFT_SUPPORT
#define XftNfontName	"fontname"
#endif

typedef struct _DclockRec *DclockWidget;
typedef struct _DclockClassRec *DclockWidgetClass;

extern WidgetClass dclockWidgetClass;

#endif /* _XtDclock_h */
