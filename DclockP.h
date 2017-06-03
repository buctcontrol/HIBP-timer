/*
 * Dclock.c -- a digital clock widget.
 * Copyright (c) 1988 Dan Heller <argv@sun.com>
 */
#ifndef _XtDclockP_h
#define _XtDclockP_h

#include <X11/CoreP.h>
#include "Dclock.h"

/* Add -DXFT_SUPPORT in the Imakefile for freefont support */
#include <X11/Xft/Xft.h>


typedef struct {
    Pixel      		foreground;
    Pixel      		led_off;
    Pixel      		background;
    Boolean		tails;		/* put tails on 6 & 9 */
    Boolean		scroll;
    Boolean		fade;
    int			fade_rate;	/* millisec. betw. fade steps */
    float		angle;		/* rise over run */
    float		width_factor;   /* ratio of digit to segment width */
    float		small_ratio;	/* ratio of small to large digits */
    float		sec_gap;	/* gap between normal digits and
					   seconds, as ratio to digit width */
    float		space_factor;   /* ratio of digit width to border sp.*/
    Boolean		seconds;
    Boolean		blink;		/* colon blinks the seconds */
    Boolean		bell;
    Boolean		miltime;
    Boolean		utc;
    Boolean		display_time;	/* when false, display alarm time */
    Boolean		alarm;		/* alarm goes off at alarm time */
    String		alarm_time;	/* time alarm goes off "14:30:00" */
    Boolean		alarm_persist;  /* stays in reverse vid after alarm */
    String		date_fmt;
    Boolean		dateup;
    String		alarmfile;	/* audio file to play for alarm */
    String		bellfile;	/* audio file to play for hour bell */
    String		audioplay;	/* executable to use to play bell */
    XFontStruct		*font;

#ifdef XFT_SUPPORT
    String		xftfontname;
    XftFont		*xftfont;
    XftDraw		*xftdraw;
    XftColor		xftbg;
    XftColor		xftfg;
#endif

    /* non-resources (e.g. user can't set) */
    XtIntervalId	interval_id;
    XtIntervalId	punt_resize;
    GC			foreGC, backGC;
    float		digit_w, digit_h;
    Pixmap		digit_one[2];	/* "1" or blank, for civilian time */
    Pixmap		digits[10];
    Pixmap		tiny_digits[10];
    Pixmap		colon[2];
    Boolean		holdoff;	/* Resize/Expose holdoff */
} DclockPart;

typedef struct _DclockRec {
    CorePart	core;
    DclockPart	dclock;
} DclockRec;

typedef struct {int dummy;} DclockClassPart;

typedef struct _DclockClassRec {
    CoreClassPart	core_class;
    DclockClassPart	dclock_class;
} DclockClassRec;

extern DclockClassRec dclockClassRec;

#endif /* _XtDclockP_h */
