/*
 * Copyright (c) 1988 Dan Heller <argv@sun.com>
 * dclock -- program to demonstrate how to use the digital-clock widget.
 * To specify a date, the date format is a string corresponding to the
 * syntax of strftime(3).
 * To specify seconds to be displayed, use "-seconds" or use the resource
 * manager: *Dclock.seconds: on
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <locale.h>
#include <X11/Intrinsic.h>
#include <wiringPi.h>
#include "uartman.h"
#include "Dclock.h"

#define WITH_X_WINDOWS          1

#define LOOP_NULL               0x0
#define LOOP_READY              0x1
#define LOOP_COUNT_DISPLAY      0x7
#define LOOP_COUNT_NODISPLAY    0x3

#define READY_SEND_WAITECHO     0x0
#define READY_SEND_ECHOED       0x1
#define READY_RECV_WAIT         0x2
#define READY_RECV_ECHOED       0x3

#define     TRIGGER_READY   7   //发车准备
#define     TRIGGER_SWITCH  3   //红外对射
#define     TRIGGER_DISPLAY 0   //恢复显示

char groupCmdHead[45] = {'A','T','+','D','M','O','S','E','T','G','R','O','U','P','=','0',',',
	                        '1','5','5','.','0','0','0','0',',',        
	                        '1','5','5','.','0','0','0','0',',',
	                        '0',',','2',',',
                            '1','2',',',
                            '0','\r','\n'  };
char groupAnswer[16] ={'+','D','M','O','S','E','T','G','R','O','U','P',':','0','\r','\n'};        // 
																	//+DMOSETGROUP:0   成功 
																	//+DMOSETGROUP:1   数据设置超出范围
																	
char sendMsgHead[10] ={'A','T','+','D','M','O','M','E','S','='};                                 //AT+DMOMES=[Message Lenth]XXXXX
char msgAnswerSuccess[11] ={'+','D','M','O','M','E','S',':','0','\r','\n'};                      //发射方应答 +DMOMES:0  发送成功 
char msgAnswerFail[11] ={'+','D','M','O','M','E','S',':','1','\r','\n'};                         //发射方应答 +DMOMES:1  发送失败 
char sendMsgReady[14] ={'A','T','+','D','M','O','M','E','S','=', 1, '2','\r','\n'};              //AT+DMOMES=12
char sendMsgStart[14] ={'A','T','+','D','M','O','M','E','S','=', 1, '3','\r', '\n'};             //AT+DMOMES=13
char connectCmd[15] = {'A','T','+','D','M','O','C','O','N','N','E','C','T','\r','\n'};           //AT+DMOCONNECT
char connectAnswerSuccess[15] = {'+','D','M','O','C','O','N','N','E','C','T',':','0','\r','\n'}; //+DMOCONNECT:0
char connectAnswerFail[15] = {'+','D','M','O','C','O','N','N','E','C','T',':','1','\r','\n'};    //+DMOCONNECT:1

unsigned int g_ready_interrupt   = 0;
unsigned int g_switch_interrupt  = 0;
unsigned int g_display_interrupt = 0;

unsigned long start_flag_time = 0;
unsigned long last_start_flag_time = 0;
int last_switch_val = 0;        //记录对射上次状态，防抖用
unsigned long switch_flag_time;
unsigned long last_switch_flag_time = 0;
int last_display_val = 1;       //记录显示开关上次状态，防抖用
unsigned long display_flag_time;
unsigned long last_display_flag_time = 0;
unsigned long first_time;
unsigned long now_time;
unsigned long time_dest;
unsigned int tm_min;
unsigned int tm_sec;
unsigned int tm_milliseconds;
unsigned int g_status = LOOP_NULL;
unsigned int g_ready_status = READY_SEND_WAITECHO;
unsigned char tm_milliseconds_number1 = 0;
unsigned char tm_milliseconds_number2 = 0;
unsigned char tm_sec_number1 = 0;
unsigned char tm_sec_number2 = 0;
unsigned char tm_min_number1 = 0;
unsigned char tm_min_number2 = 0;
char buffer[20];
int ret = 0;
unsigned int setup_flag = 1;
static pthread_mutex_t g_mutInt = PTHREAD_MUTEX_INITIALIZER;

static XrmOptionDescRec options[] = {
    {"-date",	   "*Dclock.date",	  XrmoptionSepArg, NULL    },
    {"-dateup",    "*Dclock.dateUp",	  XrmoptionNoArg,  "TRUE"  },
    {"-nodateup",  "*Dclock.dateUp",	  XrmoptionNoArg,  "FALSE" },
    {"-seconds",   "*Dclock.seconds",	  XrmoptionNoArg,  "TRUE"  },
    {"-miltime",   "*Dclock.militaryTime",XrmoptionNoArg,  "TRUE"  },
    {"-nomiltime", "*Dclock.militaryTime",XrmoptionNoArg,  "FALSE" },
    {"-stdtime",   "*Dclock.militaryTime",XrmoptionNoArg,  "FALSE" },
    {"-utc",       "*Dclock.utcTime",     XrmoptionNoArg,  "TRUE"  },
    {"-noutc",     "*Dclock.utcTime",     XrmoptionNoArg,  "FALSE" },
    {"-disptime",  "*Dclock.displayTime", XrmoptionNoArg,  "TRUE"  },
    {"-audioPlay", "*Dclock.audioPlay",	  XrmoptionSepArg, NULL    },
    {"-bell",	   "*Dclock.bell",	  XrmoptionNoArg,  "TRUE"  },
    {"-nobell",	   "*Dclock.bell",	  XrmoptionNoArg,  "FALSE" },
    {"-bellFile",  "*Dclock.bellFile",	  XrmoptionSepArg, NULL    },
    {"-alarm",     "*Dclock.alarm",	  XrmoptionNoArg,  "TRUE"  },
    {"-noalarm",   "*Dclock.alarm",	  XrmoptionNoArg,  "FALSE" },
    {"-persist",   "*Dclock.alarmPersist",XrmoptionNoArg,  "TRUE"  },
    {"-nopersist", "*Dclock.alarmPersist",XrmoptionNoArg,  "FALSE" },
    {"-alarmTime", "*Dclock.alarmTime",	  XrmoptionSepArg, NULL    },
    {"-alarmFile", "*Dclock.alarmFile",	  XrmoptionSepArg, NULL    },
    {"-blink",     "*Dclock.blink",	  XrmoptionNoArg,  "TRUE"  },
    {"-noblink",   "*Dclock.blink",	  XrmoptionNoArg,  "FALSE" },
    {"-tails",     "*Dclock.tails",	  XrmoptionNoArg,  "TRUE"  },
    {"-notails",   "*Dclock.tails",	  XrmoptionNoArg,  "FALSE" },
    {"-scroll",    "*Dclock.scroll",	  XrmoptionNoArg,  "TRUE"  },
    {"-noscroll",  "*Dclock.scroll",	  XrmoptionNoArg,  "FALSE" },
    {"-fade",	   "*Dclock.fade",	  XrmoptionNoArg,  "TRUE"  },
    {"-nofade",	   "*Dclock.fade",	  XrmoptionNoArg,  "FALSE" },
    {"-fadeRate",  "*Dclock.fadeRate",	  XrmoptionSepArg, NULL    },
    {"-slope",	   "*Dclock.angle",	  XrmoptionSepArg, NULL    },
    {"-thickness", "*Dclock.widthFactor", XrmoptionSepArg, NULL    },
    {"-smallsize", "*Dclock.smallRatio",  XrmoptionSepArg, NULL    },
    {"-spacing",   "*Dclock.spaceFactor", XrmoptionSepArg, NULL    },
    {"-second_gap","*Dclock.secondGap",	  XrmoptionSepArg, NULL    },
    {"-led_off",   "*Dclock.led_off",	  XrmoptionSepArg, NULL    },
#ifdef XFT_SUPPORT
    {"-fontName",  "*Dclock.fontName",	  XrmoptionSepArg, NULL    },
#endif
};

static void
Usage(name, args)
String name, *args;
{
    static char *help_message[] = {
	"where options include:",
	"    -help			print this help text",
	"    -bg color			field background color",
	"    -fg color			segment foreground color",
	"    -fn font			font name",
	"    -geometry geom		size of mailbox",
	"    -display host:dpy		X server to contact",
	"    -led_off color		segment background color",
	"    -date \"date format\"	show the date in strftime(3) format",
	"    -[no]dateup 		[don't]	put the date up at the top",
	"    -[no]seconds 		[don't]	display seconds",
	"    -[no]miltime 		[don't]	display time in 24 hour format",
	"    -[no]utc			[don't] display the UTC time instead of local",
	"    -[no]blink			[don't] blink the colon",
	"    -[no]scroll 		turn on [off] scrolling of numbers",
	"    -[no]tails			draw [remove] tails on digits 6 and 9",
	"    -[no]fade			[don't] fade numbers",
	"    -[no]bell 			[don't] ring bell each half hour",
	"    -[no]persist		[don't] leave in reverse video after alarm",
	"    -bellFile filename		sound file for bell sound",
	"    -[no]alarm 		turn on/off alarm",
	"    -alarmTime hh:mm		time alarm goes off",
	"    -alarmFile filename	sound file for alarm sound",
        "    -audioPlay	filename	executable to use to play bell and alarm",
	"    -fadeRate int_val		wait between fade steps (in msec)",
	"    -slope float_val		set angle of the digits",
	"    -smallsize float_val	set size ratio of the small to large digits",
	"    -second_gap float_val	set spacing between minutes and seconds digits",
	"    -thickness	float_val	set segment thickness as ratio to digit width",
	"    -spacing float_val		set digit spacing as ratio to digit width",
#ifdef XFT_SUPPORT
	"    -fontName name		name of freefont font to use for date string",
#endif
	NULL
    };
    char **cpp;

    fprintf(stderr, "Invalid Args:");
    while (*args)
	fprintf(stderr, " \"%s\"", *args++);
    fputc('\n', stderr);
    fprintf(stderr, "usage: %s [-options ...]\n", name);
    for (cpp = help_message; *cpp; cpp++)
	fprintf(stderr, "%s\n", *cpp);
    exit(1);
}

static void
quit()
{
    exit(0);
}

static XtActionsRec actionsList[] = {
    { "quit",	quit },
};

void *displayThread(void *pApp)
{
    XtAppContext appDisplay;

    memcpy(&appDisplay, pApp, sizeof(appDisplay));
    XtAppMainLoop(appDisplay);

    return NULL;
}

void *timeThread(void *pApp)
{
    struct timeval now;
    static int sec;

    do{
        maketimeofday(&now, NULL);
        if(now.tv_usec/10000 != sec)
        {
            printf("%02d:%02d.%02d\n", (now.tv_sec/60)%60, now.tv_sec%60, now.tv_usec/10000);
            sec = now.tv_usec/10000;
        }
        usleep(100);
    }while(1);

    return NULL;
}

int maketimeofday(struct timeval *tv, struct timezone *tz)
{
    int ret = 0;

    tv->tv_sec  = time_dest/1000;
    tv->tv_usec = (time_dest%1000)*1000;
    
    return ret;
}

long getRacingTime()
{
    unsigned char buf[64];
    unsigned char buffer;
    struct timeval tv;
    int i = 0;
    long value = 0;
    int ret = APP_SUCCESS;

    memset(&buf, 1, sizeof(buf));
    do
    {
        tv.tv_sec  = 0;
        tv.tv_usec = 30;
        ret = uartManPortCanRead(&tv);
        if(ret == 1)
        {
            ret = uartManPort232ReadByte(&buffer);       
            if(ret == APP_SUCCESS)
            {
                memcpy(&buf[i], &buffer, sizeof(buffer));
                i++;
                //printf("%c", buffer);
            }
            if(buffer == '\n')
            {
                value = atol(buf);
                //printf("\nvalue = %ld\n", value);
                ret = APP_SUCCESS;
                break;
            }
        }
    }while(1);

    return value;
}

main(argc, argv)
char *argv[];
{
    XtAppContext app;
    Widget toplevel, clock_w;
    char *name, *rindex();
    XWMHints     *wmhints;       /* for proper input focus */
    pthread_attr_t attr;
    pthread_t tid;
    int i = 0;
    unsigned char buf[16];
    struct timeval tv;


    if (name = rindex(argv[0], '/'))
	name++;
    else
	name = argv[0];

#ifdef WITH_X_WINDOWS
    setlocale(LC_TIME, "");

    toplevel = XtAppInitialize(&app, "Dclock", options, XtNumber(options),
			&argc, argv, (String *)NULL, (ArgList)NULL, 0);
    XtAppAddActions(app, actionsList, 1);

    if (argc != 1)
	Usage(name, argv+1);

    clock_w = XtCreateManagedWidget(name, dclockWidgetClass, toplevel, NULL, 0);
    XtOverrideTranslations(clock_w, XtParseTranslationTable("<Key>q: quit()"));

    XtRealizeWidget(toplevel);

    wmhints = XGetWMHints(XtDisplay(toplevel), XtWindow(toplevel));
    wmhints->input = True;
    wmhints->flags |= InputHint;
    XSetWMHints(XtDisplay(toplevel), XtWindow(toplevel), wmhints);
    XFree(wmhints);

    pthread_attr_init(&attr);
    if(pthread_create(&tid, &attr, displayThread,(void*)&app))
	{
		printf("create display thread  failed!\n");
        goto EXIT0;
	}
#else
    pthread_attr_init(&attr);
    if(pthread_create(&tid, &attr, timeThread,(void*)&app))
    {
	    printf("create display thread  failed! %s\n", strerror(errno));
        goto EXIT0;
    }
#endif

    pthread_mutex_init(&g_mutInt, NULL);

#ifdef WITH_UART
    ret = uartManInit();
    if (ret)
    {
        dbgPrintfl(WARN,"UART module init fail!\n");
	goto EXIT0;
    }
#endif

    tm_milliseconds_number1 = 8;
    tm_milliseconds_number2 = 8;
    tm_sec_number1 = 8;
    tm_sec_number2 = 8;
    tm_min_number1 = 8;
    tm_min_number2 = 8;

    while(1)
    {
        time_dest = getRacingTime();
    }

EXIT0:

    return 0;
}

