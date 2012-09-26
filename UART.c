#include     <stdio.h>      /*标准输入输出定义*/
#include     <stdlib.h>     /*标准函数库定义*/
#include     <unistd.h>     /*Unix 标准函数定义*/
#include     <sys/ioctl.h>
#include     <sys/types.h>
#include     <sys/stat.h>
#include     <fcntl.h>      /*文件控制定义*/
#include     <termios.h>    /*PPSIX 终端控制定义*/
#include     <errno.h>      /*错误号定义*/

#include        "UART.h"

//---------------------------------------------------------------

//******************************************************************

#define    TRUE    0
#define    FALSE    -1

//---------------------------------------------------------------------------
/***@brief  设置串口通信速率
*@param  fd     类型 int  打开串口的文件句柄
*@param  speed  类型 int  串口速度
*@return  void*/

static    int speed_arr[] = { B115200, B57600, B38400, B19200, B9600, B4800, B2400, B1200, B300,
                              B115200, B57600, B38400, B19200, B9600, B4800, B2400, B1200, B300,
                            };
//  这是因为在linux下，系统为波特率专门准备了一张表用B38400,B19200......
static    int name_arr[] = {115200, 57600, 38400, 19200, 9600, 4800, 2400, 1200, 300,
                            115200, 57600, 38400, 19200, 9600, 4800, 2400, 1200, 300,
                           };
static    void     set_speed(int fd, int speed)
{
    int     i;
    int     status;
    struct termios     Opt; //定义了这样一个结构

    tcgetattr(fd, &Opt);     //用来得到机器原端口的默认设置
    for (i = 0; i < sizeof(speed_arr) / sizeof(int); i++)
    {
        if (speed == name_arr[i])        //判断传进来是否相等
        {
            tcflush(fd, TCIOFLUSH);             //刷新输入输出缓冲
            cfsetispeed(&Opt, speed_arr[i]);     //这里分别设置
            cfsetospeed(&Opt, speed_arr[i]);

            // Opt.c_cflag |= (CLOCAL | CREAD);

            status = tcsetattr(fd, TCSANOW, &Opt);     //这是立刻把bote rates设置真正写到串口中去
            if (status != 0)
            {
                perror("uart set_speed err");         //设置错误
                return;
            }
            tcflush(fd, TCIOFLUSH);                    //同上
            break;
        }
    }
}

//---------------------------------------------------------------
/**
*@brief   设置串口数据位，停止位和效验位
*@param  fd     类型  int  打开的串口文件句柄
*@param  databits 类型  int 数据位   取值 为 7 或者8
*@param  stopbits 类型  int 停止位   取值为 1 或者2
*@param  parity  类型  int  效验类型 取值为N,E,O,,S
*/

static    int     set_Parity(int fd, int databits, int stopbits, int parity)
{
    struct termios     options;     //定义一个结构

    if (tcgetattr(fd, &options) != 0)   //首先读取系统默认设置options中,必须
    {
        perror("uart set_Parity tcgetattr err");
        return(FALSE);
    }

    options.c_cflag &= ~CSIZE;         //这是设置c_cflag选项不按位数据位掩码
    switch (databits) /*设置数据位数*/
    {
    case 7:
        options.c_cflag |= CS7;     //设置c_cflag选项数据位为7位
        break;
    case 8:
        options.c_cflag |= CS8;     //设置c_cflag选项数据位为8位
        break;
    default:
        fprintf(stderr, "uart Unsupported data size\n");    //其他的都不支持
        return (FALSE);
    }
    switch (parity) //设置奇偶校验，c_cflag和c_iflag有效
    {
    case 'n':
    case 'N':
        options.c_cflag &= ~PARENB;     /* Clear parity enable */
        options.c_iflag &= ~INPCK;         /* Enable parity checking */
        break;
    case 'o':     //奇校验 其中PARENB校验位有效；PARODD奇校验INPCK检查校验
    case 'O':
        options.c_cflag |= (PARODD | PARENB);    /* 设置为奇效验*/
        options.c_iflag |= INPCK;                 /* Disnable parity checking */
        //options.c_iflag |=(INPCK|ISTRIP);
        break;
    case 'e':
    case 'E':     //偶校验，奇校验不选就是偶校验了
        options.c_cflag |= PARENB;         /* Enable parity */
        options.c_cflag &= ~PARODD;     /* 转换为偶效验*/
        options.c_iflag |= INPCK;         /* Disnable parity checking */
        //options.c_iflag |=(INPCK|ISTRIP);
        break;
    case 'S':
    case 's':      /*as no parity*/
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        //options.c_iflag &= ~INPCK;
        break;
    default:
        fprintf(stderr, "uart Unsupported parity\n");
        return (FALSE);
    }

#if    0
    /* Set input parity option */
    if (parity != 'n')             //这是设置输入是否进行校验
        options.c_iflag |= INPCK;
#endif

    /* 设置停止位*/
    switch (stopbits)         //这是设置停止位数，影响的标志是c_cflag
    {
    case 1:
        options.c_cflag &= ~CSTOPB;     // 不指明表示一位停止位
        break;
    case 2:
        options.c_cflag |= CSTOPB;         //指明CSTOPB表示两位，只有两种可能
        break;
    default:
        fprintf(stderr, "uart Unsupported stop bits\n");
        return (FALSE);
    }

//--------------------------------------------------------------------------------------------
//    tcflush(fd,TCIFLUSH);

    //RAW MODE
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);     /*Input*/    //无需回车直接输出
    options.c_oflag  &= ~OPOST;                          /*Output*/

    //设置控制字符和超时参数的，一般默认即可
    options.c_cc[VTIME] = 150;         // 15 seconds
    options.c_cc[VMIN] = 0;            // 最少读取的字符数

    options.c_iflag &= ~(IXON | IXOFF | IXANY);        //无软件流控制

#if    1
    //回车和换行不看成一个字符
    options.c_oflag &= ~(INLCR | IGNCR | ICRNL);
    options.c_oflag &= ~(ONLCR | OCRNL);
#endif

    options.c_cflag |= (CLOCAL | CREAD);        //CLOCAL  : 本地连接，无调制解调器控制
    // CREAD   : 允许接收数据
//IGNPAR  : ignore bytes with parity errors
//ICRNL   : map CR to NL (otherwise a CR input on the other computer will not
//CRTSCTS : 输出硬件流控(只能在具完整线路的缆线下工作，参考 Serial-HOWTO 第七节)
//---------------------------------------------------------------------------
//-------------------------------------------------------------------------------

    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &options) != 0)       /* Update the options and do it NOW */
    {
        perror("SetupSerial err");
        return (FALSE);
    }

    tcflush(fd, TCIOFLUSH);

    return (TRUE);
}


//***************************************************************************

static    int     fd = -1;

/**
*@brief 打开串口
*/
int        OpenUart(char    *dev)
{

    /*
    开启设备用于读写，但是不要以控制 tty 的模式，
    因为我们并不希望在发送 Ctrl-C 后结束此进程
    */
// fd = open(dev, O_RDWR | O_NOCTTY );

    fd = open(dev, O_RDWR);                //| O_NOCTTY | O_NDELAY这种方式看open函数
    if (fd == -1)
    {
        printf("Can't Open Serial Port\n");
        return -1;
    }
    return    0;
}
void        CloseUart(void)
{
    close(fd);
}



//=================================
#define    FOR_IMX233_PTZ

#ifndef    FOR_IMX233_PTZ
int    UartWrite(char *buffer, int length)
{
    return    write(fd, buffer , length);

}
#else

extern    int    write_ptz(char *buffer, int length);
extern    int    read_ptz(char *buffer, int length);

int    UartWrite(char *buffer, int length)
{
    //printf("linux UartWrite---length= %d, \n",length);
    //printf("linux UartWrite-----buffer = %x,%x,%x,%x,%x,%x,%x,%x \n",buffer[0],buffer[1],buffer[2],buffer[3],buffer[4],buffer[5],buffer[6],buffer[7]);

    return    write_ptz(buffer , length);

}
extern    int    UartWrite(char *buffer, int length);

#endif
//==================================================



int    UartWriteChar(char *buffer)
{
    return    write(fd, buffer , 1);

}

int    UartRead(char *buffer, int length)
{
    int    nread;

    nread = read(fd, buffer, length);
    return    nread;
}

int    UartReadCharPolling(char *buffer)
{
    int    nread;

    nread = read(fd, buffer, 1);
    return    nread;    //nread 是否为0
}

//------------------------------------------------------


int        SetUart(int speed, int databits, int stopbits, int parity)
{
    set_speed(fd, speed);

    if (set_Parity(fd,  databits, stopbits,  parity) == FALSE)
    {
        printf("Set Parity Error\n") ;
        return     -1 ;
    }
    return    0 ;
}

//------------------------------------------------------
#define TIOCSBRK    0x5427  /* BSD compatibility */

int        SetUartDirection(int    direction)
{
    int    ret;

    return 0 ;            //硬件强行设置DE 高电平

    /*use TIOCSBRK to control DE. 0 - low, 1 - high*/
    ret = ioctl(fd, TIOCSBRK, direction);
    if (ret < 0)
    {
        perror("uart TIOCSBRK error");
        return    -1;
    }
    return    0;

}
//*****************************************************************************************************

//****************************************************************************

//首先定义uart_driver结构。形如

/* static struct uart_driver clps711x_reg = {
 .driver_name  = "ttyCL",
 .dev_name  = "ttyCL",
 .major   = SERIAL_CLPS711X_MAJOR,//主设备号，自定义或者查表，这两个都是define
 .minor   = SERIAL_CLPS711X_MINOR,//次设备号
 .nr   = UART_NR,//可驱动的uart数，一个设备可对应多个uart
 .cons   = CLPS711X_CONSOLE,//
};
*/
// 还要定义每个口的参数，形如
// 每个口的ops是个指向实际操作结构的指针。其定义如下，这个最重要
//之后需要注册到系统

/* 剩下的就补足ops结构里的各项操作。
如果使用中断，那么要在startup里注册中断及对应处理操作
对于发送接收的实际数据，其实还是有些麻烦的
需要发送的数据储存在circ_buf结构里，可以从 uart_port->info->xmit;里得到它
xmit->buf[xmit->tail]取得当前字符。
if (uart_circ_empty(xmit)) 可判断是否结束。
接收到的字符需要送给tty驱动去处理，uart_port->info->tty可以得到tty驱动的入口。
tty_insert_flip_char(tty, ch, flg);
可以将字符插入到tty中，但是要注意，tty中的buff是有限的，
用
if (tty->flip.count >= TTY_FLIPBUF_SIZE)来判断
最后需要tty_flip_buffer_push(tty);

*/

#ifndef FOR_IMX233_PTZ
void SetUartSpeed(int speed)
{
    set_speed(fd, speed);
}
#else
void SetUartSpeed(int speed)
{
    return;
}
#endif


