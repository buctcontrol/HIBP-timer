/* �����ļ������ʽΪgb2312,cp936  */
/** $Id$ $DateTime$
*  @file uartman.c
*  @brief ���ڹ���ģ��
*  @version 0.0.1
*  @since 0.0.1
*  @author duwei<duwei@bstar.com.cn>
*  @date 2009-08-04    create
*/
/******************************************************************************
*@note
Copyright 2007, BeiJing Bluestar Corporation, Limited
ALL RIGHTS RESERVED
Permission is hereby granted to licensees of BeiJing Bluestar, Inc. products
to use or abstract this computer program for the sole purpose of implementing
a product based on BeiJing Bluestar, Inc. products. No other rights to
reproduce, use, or disseminate this computer program,whether in part or in
whole, are granted. BeiJing Bluestar, Inc. makes no representation or
warranties with respect to the performance of this computer program, and
specifically disclaims any responsibility for any damages, special or
consequential, connected with the use of this program.
For details, see http://www.bstar.com.cn/
*******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef __USE_UNIX98
    #define __USE_UNIX98 /**< ʹ�õݹ�ʽ������ */
#endif
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <sys/time.h>
#include <dirent.h>
#include <dlfcn.h>
#include "uartman.h"

#undef  DBG_MOD_ID
#define DBG_MOD_ID GET_DBG_MID(ADMIND_ID, ADMIND_UART_ID)

extern int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind);

#ifndef CMSPAR
#define CMSPAR 010000000000
#endif 

static int g_h232 = -1;
static pthread_mutex_t g_mut232 = PTHREAD_MUTEX_INITIALIZER;

static void uartManClosePorts();
static int uartManPortSetProp(int hPort, const CfgUartProp* pProp);
static int uartManBaudConversion(int nBaudRate);
static int uartManSetTermios(struct termios * pNewtio, const CfgUartProp* pProp);

/**
 * @fn readn
 * @brief Read "n" bytes from a descriptor.
 * @param[in]  fd   : file descriptor
 * @param[in]  vptr : buffer, allocated by caller   
 * @param[in]  n    : maximum bytes to be read
 * @return  the result of function. success or fail;
 * @retval  -1 :  read error
 * @retval   n :  n bytes are gotten
 * @retval  <n :  '<n' bytes are gotten, but socket closed or file ended.
 */
ssize_t readn(int fd, void *vptr, size_t n)
{
    size_t  nleft;
    ssize_t  nread;
    char  *ptr = NULL;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) 
    {
        if ( (nread = read(fd, ptr, nleft)) < 0)
        {
            if (errno == EINTR)
            {
                nread = 0;
            }
            else
            {
                return(-1);
            }
        }
        else if (nread == 0)
        {
            break; 
        }
        nleft -= nread;
        ptr   += nread;
    }
    return ((ssize_t)(n - nleft));  
}

/**
 * @fn writen
 * @brief Write "n" bytes to a descriptor
 * @param[in]  fd   : file descriptor
 * @param[in]  vptr : buffer to be written, allocated by caller   
 * @param[in]  n    : maximum bytes to be write
 * @return  the result of function. success or fail;
 * @retval  -1 :  write error
 * @retval   n :  n bytes are gotten
 * @retval  <n :  '<n' bytes are gotten, but socket closed or file ended.
 */
ssize_t writen(int fd, const void *vptr, size_t n)
{
    size_t    nleft;
    ssize_t    nwritten;
    const char  *ptr = NULL;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) 
    {
        if ( (nwritten = write(fd, ptr, nleft)) <= 0) 
        {
            if (errno == EINTR)
            {
                nwritten = 0;  
            }
            else
            {
                return(-1); 
            }
        }
        nleft -= nwritten;
        ptr   += nwritten;
    }
    return ((ssize_t)n);
}

/** 
 * @fn uartManClosePorts
 * @brief �ر����д��� 
 */
static void uartManClosePorts()
{
    if(g_h232 != -1)
    {
        close(g_h232);
        g_h232 = -1;
    }
}

/**
 * @fn  uartManBaudConversion
 * @brief  ������ת��
 * @param[in]  baudRate
 * @param[out]  ��
 * @return ����ϵͳ��׼������ֵ
 */
static int uartManBaudConversion(int nBaudRate)
{
    int nRet = APP_FAIL;

    switch (nBaudRate)
    {
        case 2400:
            nRet =  B2400;
            break;
        case 4800:
            nRet =  B4800;
            break;
        case 9600:
            nRet =  B9600;
            break;
        case 19200:
            nRet =  B19200;
            break;
        case 38400:
            nRet =  B38400;
            break;
        case 57600:
            nRet =  B57600;
            break;
        case 115200:
            nRet =  B115200;
            break;
        default:
            dbgPrintfl(FORCE, "Unsuppoted baud rate: %d", nBaudRate);
            nRet = APP_FAIL;
            break;
    }

    return nRet;
}

/**
 * @fn uartManSetTermios
 * @brief ����TERMIO
 * @param[out] pNewtio
 * @param[in] pProp: ���ڲ���
 * @return: APP_FAIL | APP_OK
 */
static int uartManSetTermios(struct termios * pNewtio, const CfgUartProp* pProp)
{
    int nRet = APP_FAIL;
    uint cflag = 0,iflag = 0;
    int baudRate = 0;

    switch (pProp->nDataBits)
    {
        case 5:
            cflag |= CS5;
            break;

        case 6:
            cflag |= CS6;
            break;

        case 7:
            cflag |= CS7;
            break;

        case 8:
            cflag |= CS8;
            break;

        default:
            dbgPrint(FORCE, "uartMan:uartManSetTermios:Invalid databits:%d\n", pProp->nDataBits);
            goto EXIT1;
    }

    baudRate = uartManBaudConversion(pProp->nBaudRate);
    dbgPrint(DETAIL, "uartSetTermios : baudRate = %d    baudRate for linux =%d\n", pProp->nBaudRate, baudRate);

    switch (pProp->nParityType)
    {
        case SERIAL_PORT_PARITY_TYPE_NONE:
            cflag |= baudRate  |  CREAD | CLOCAL;
            iflag = IGNPAR;
            break;

        case SERIAL_PORT_PARITY_TYPE_EVEN:
            cflag |= baudRate  |  CREAD | CLOCAL |PARENB;
            iflag = 0;
            break;

        case SERIAL_PORT_PARITY_TYPE_ODD:
            cflag |= baudRate  |  CREAD | CLOCAL |PARENB | PARODD;
            iflag = 0;
            break;

        case SERIAL_PORT_PARITY_TYPE_SPACE:
            cflag |= baudRate  |  CREAD | CLOCAL |PARENB | CMSPAR;
            iflag = 0;
            break;

        case SERIAL_PORT_PARITY_TYPE_MARK:
            cflag |= baudRate  |  CREAD | CLOCAL |PARENB | PARODD | CMSPAR;
            iflag = 0;
            break;

        default:
            dbgPrint(FORCE, "uartMan:uartManSetTermios:Invalid parity type:%d\n", pProp->nParityType);
            goto EXIT1;
            break;
    }

    if (pProp->nStopBits == 2)
    {
        cflag |= CSTOPB;
    }

    bzero(pNewtio, sizeof(struct termios));
    pNewtio->c_cflag = cflag;
    pNewtio->c_iflag = iflag;

    pNewtio->c_oflag = 0;
    pNewtio->c_lflag = 0; /**< non ICANON */

    pNewtio->c_cc[VINTR] = 0;     /**< Ctrl-c */
    pNewtio->c_cc[VQUIT] = 0;     
    pNewtio->c_cc[VERASE] = 0;     /**< del */
    pNewtio->c_cc[VKILL] = 0;     /**< @ */
    pNewtio->c_cc[VEOF] = 4;     /**< Ctrl-d */
    pNewtio->c_cc[VTIME] = 5;     /**< �ַ��䶨ʱ��, timeout = VTIME*0.1 */
    pNewtio->c_cc[VMIN] = 0;     /**< �ڶ���VMIN���ַ�ǰ���� */
    pNewtio->c_cc[VSWTC] = 0;    
    pNewtio->c_cc[VSTART] = 0;     /**< Ctrl-q */
    pNewtio->c_cc[VSTOP] = 0;     /**< Ctrl-s */
    pNewtio->c_cc[VSUSP] = 0;     /**< Ctrl-z */
    pNewtio->c_cc[VEOL] = 0;     /**< '\0' */
    pNewtio->c_cc[VREPRINT] = 0;     /**< Ctrl-r */
    pNewtio->c_cc[VDISCARD] = 0;     /**< Ctrl-u */
    pNewtio->c_cc[VWERASE] = 0;     /**< Ctrl-w */
    pNewtio->c_cc[VLNEXT] = 0;     /**< Ctrl-v */
    pNewtio->c_cc[VEOL2] = 0;     /**< '\0' */

    nRet = APP_OK;

EXIT1:
    return nRet;
}

/**
 * @fn uartManPortSetProp
 * @brief ���ô��ڲ���
 * @param[in] hPort: ���ھ��
 * @param[in] pPort: ����
 * return APP_OK | APP_FAIL 
 */
static int uartManPortSetProp(int hPort, const CfgUartProp* pProp)
{
    struct termios newtio;
    int nRet = APP_FAIL;

    if(hPort == -1)
    {
        dbgPrintfl(FATAL, "uart:uartManPortSetProp():Port not open!\n");
        return APP_FAIL;
    }

    nRet = uartManSetTermios(&newtio, pProp);
    /**
     * ������TCSADRAIN�Ա�֤֮ǰwrite()������ʹ������ǰ�������ʹ�Ŀ�ĵ�
     * man:TCSADRAIN:the change occurs after all output written to fd 
     * has been transmitted.  This function should be used when changing 
     * parameters that affect output.
     */
    tcsetattr(hPort, TCSADRAIN, &newtio); 
    nRet = APP_OK;

    return nRet;
}

/**
 * @fn uartManPortCanRead
 * @brief: ���Դ����Ƿ�ɶ�
 * @param[in] timeout: ֱ�Ӵ��ݸ�select����ֵ�ᱻselect�޸ġ�
 * @return: ��ȷ���ɶ�����APP_OK | ��ʱ�������APP_FAIL
 * @remarks: ��������, ��������������߳��ִ���
 */
int uartManPortCanRead(struct timeval* timeout)
{
#ifdef WITH_UART
    fd_set rdfds;
    int nRet = APP_OK;

    if(g_h232 == -1)
    {
        dbgPrintfl(WARN, "port not open!\n");
        nRet = APP_FAIL;
        goto EXIT0;
    }

    FD_ZERO(&rdfds);
    FD_SET(g_h232, &rdfds);

    nRet = select(g_h232 + 1, &rdfds, NULL, NULL, timeout);
    if(nRet != 1)
    {
        //dbgPrintfl(WARN, "select failed: ret = %d\n", nRet);
        nRet = APP_FAIL;
    }
    if(nRet == 0)
    {
        dbgPrintfl(WARN, "select timeout\n");
    }

EXIT0:

    return nRet;
#else
    return 0;
#endif
}

/**
 * @fn uartManInit
 * @brief: ���ڹ���ģ���ʼ��, �򿪴���(rs232��rs485, ����sysconfig�е����ݽ�������)
 * @return: 0 - APP_OK | APP_FAIL
 * @remarks: ϵͳ�о��ж������, 1������ʵЧ��Ӧ��Ӱ���������ڹ���, ���Գ�ʼ��������
 *			�㷨Ϊ�κ�һ�����ڳ�ʼ���ɹ���Ϊ��ʼ���ɹ�, ֻ�����д��ڶ���ʼ��ʧ�ܲ�
 *			��Ϊ��ʼ��ʧ�ܡ�Ȼ��ÿ�δ��ڲ�����Ҫ�����ж���ص��豸�������Ƿ���Ч
 */
int uartManInit()
{
    int nRet = APP_FAIL;
    CfgUartProp cfgUartProp;

    g_h232 = -1;

    cfgUartProp.nBaudRate = 9600;
    cfgUartProp.nDataBits = 8;
    cfgUartProp.nFlowCtrl = 0;
    cfgUartProp.nParityType = SERIAL_PORT_PARITY_TYPE_NONE;
    cfgUartProp.nStopBits = 1;

    do 
    {
        g_h232 = open(UM_DEV_PATH_NAME_232, O_RDWR);
        if(g_h232 == -1)
        {
            dbgPrint(FATAL, "uartMan:init:open 232 port %s failed:%s\n", 
                     UM_DEV_PATH_NAME_232, strerror(errno));
            nRet = APP_FAIL;
            break;
        }
        else
        {
            nRet = uartManPortSetProp(g_h232, &cfgUartProp);
            if (nRet != APP_OK)
            {
                dbgPrint(FATAL, "uartManPortSetProp g_h232 return 0x%x", nRet);
            }
        }

        nRet = APP_OK;
    } while (0);

    if(nRet != APP_OK)
    {
        nRet = APP_FAIL;
        uartManClosePorts();
    }
    else
    {
        pthread_mutexattr_t mutAttr;

        pthread_mutexattr_settype(&mutAttr, PTHREAD_MUTEX_RECURSIVE_NP);
        pthread_mutex_init(&g_mut232, &mutAttr);
        pthread_mutex_init(&g_mut232, NULL);
    }

    return nRet;
}

/**
 * @fn uartManPort232Write
 * @brief: ��232����д����
 * @param[in] pData: ���ݻ�����
 * @param[in] nSize: ���ݳ���
 * @return APP_OK | APP_FAIL
 */
int uartManPort232Write(const uchar* pData, int nSize)
{
#ifdef WITH_UART
    int nRet = APP_FAIL;

    if(g_h232 == -1)
    {
        dbgPrintfl(WARN, "g_h232:%d err\n", g_h232);
        nRet = APP_FAIL;
        goto EXIT0;
    }

    pthread_mutex_lock(&g_mut232);
    nRet = writen(g_h232, pData, nSize);
    pthread_mutex_unlock(&g_mut232);

    if(nRet != nSize)
    {
        dbgPrintfl(WARN, "writen err, nRet: %d, nSize: %d\n", nRet, nSize);
        nRet = APP_FAIL;
        goto EXIT0;
    }

    nRet = APP_SUCCESS;

EXIT0:
    return nRet;
#else
    return 0;
#endif
}

/**
 * @fn uartManPort232ReadByte
 * @brief: ��232���ڶ�1�ֽ�
 * @param[out] pByte: 
 * @return APP_OK | ��ش�����
 */
int uartManPort232ReadByte(uchar* pByte)
{
    int nRet = APP_FAIL;

    if(g_h232 == -1)
    {
        nRet = APP_FAIL;
        goto EXIT0;
    }

    pthread_mutex_lock(&g_mut232);
    nRet = readn(g_h232, pByte, 1);
    pthread_mutex_unlock(&g_mut232);

    if(nRet != 1)
    {
        nRet = APP_FAIL;
        goto EXIT0;
    }

    nRet = APP_SUCCESS;

EXIT0:
    return nRet;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

