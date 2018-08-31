/** $Id$ $DateTime$
*  @file uartman.h
*  @brief ���ڹ���ģ��ͷ�ļ�
*  @version 0.0.1
*  @since 0.0.1
*  @author lzj@hbike.net>
*  @date 2009-08-04    create
*/
#ifndef _UARTMAN_H_
#define _UARTMAN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned short int ushort;
typedef unsigned long int ulong;
typedef unsigned long long int ullong;
typedef long long int llong;

#define MAX_CHANNEL (16)
enum
{
    FATAL = 1, /**<KIT_DEBUG_LEVEL_FATAL*/
    WARN,      /**<KIT_DEBUG_LEVEL_WARNING*/
    TRACE,     /**<KIT_DEBUG_LEVEL_TRACE*/
    DETAIL,    /**<KIT_DEBUG_LEVEL_DETAIL*/
    FORCE,     /**<KIT_DEBUG_LEVEL_FORCE*/
};

/* ͨ�õĴ����� */
#define    APP_OK                            0
#define    APP_SUCCESS                       0
#define    APP_FAIL                        (-1)

#define dbgPrint(lev, fmt, args...)\
do{\
    printf(fmt, ##args);\
}while(0)

#define dbgPrintfl(lev, fmt, args...)\
do{\
    printf("%s:%d ", __FILE__, __LINE__);\
    printf(fmt, ##args);\
}while(0)

#define dbgAssert(x) assert(x)

//#define     WITH_UART   1

#define UM_DEV_PATH_NAME_232 "/dev/ttyACM0"

#define     SERIAL_PORT_PARITY_TYPE_NONE    0
#define     SERIAL_PORT_PARITY_TYPE_ODD     1
#define     SERIAL_PORT_PARITY_TYPE_EVEN    2
#define     SERIAL_PORT_PARITY_TYPE_MARK    3
#define     SERIAL_PORT_PARITY_TYPE_SPACE   4

/** struct UartProp
*  @brief:  ��������, 232��485һ��, ����BSCPЭ��SerialCfg��������ʵ��
*			ref. BSCP_DVX7-A(ϵͳ����).pdf p.47
*  @nParityType: У�鷽ʽ:NONE(0)|EVEN(1)|ODD(2)| MARK(3)|SPACE(4)
*  @nFlowCtrl:   ����: NONE(0)|XONOFF(1)
*/
typedef struct  
{
	uint nBaudRate; 
	char nDataBits;
	char nStopBits;
	char nParityType;
	char nFlowCtrl;
} CfgUartProp;

int uartManInit();
int uartManPortCanRead(struct timeval* timeout);
int uartManPort232ReadByte(uchar* pByte);
int uartManPort232Write(const uchar* pData, int nSize);

#endif /* _H_ */
