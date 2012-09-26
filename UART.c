#include     <stdio.h>      /*��׼�����������*/
#include     <stdlib.h>     /*��׼�����ⶨ��*/
#include     <unistd.h>     /*Unix ��׼��������*/
#include     <sys/ioctl.h>
#include     <sys/types.h>
#include     <sys/stat.h>
#include     <fcntl.h>      /*�ļ����ƶ���*/
#include     <termios.h>    /*PPSIX �ն˿��ƶ���*/
#include     <errno.h>      /*����Ŷ���*/

#include        "UART.h"

//---------------------------------------------------------------

//******************************************************************

#define    TRUE    0
#define    FALSE    -1

//---------------------------------------------------------------------------
/***@brief  ���ô���ͨ������
*@param  fd     ���� int  �򿪴��ڵ��ļ����
*@param  speed  ���� int  �����ٶ�
*@return  void*/

static    int speed_arr[] = { B115200, B57600, B38400, B19200, B9600, B4800, B2400, B1200, B300,
                              B115200, B57600, B38400, B19200, B9600, B4800, B2400, B1200, B300,
                            };
//  ������Ϊ��linux�£�ϵͳΪ������ר��׼����һ�ű���B38400,B19200......
static    int name_arr[] = {115200, 57600, 38400, 19200, 9600, 4800, 2400, 1200, 300,
                            115200, 57600, 38400, 19200, 9600, 4800, 2400, 1200, 300,
                           };
static    void     set_speed(int fd, int speed)
{
    int     i;
    int     status;
    struct termios     Opt; //����������һ���ṹ

    tcgetattr(fd, &Opt);     //�����õ�����ԭ�˿ڵ�Ĭ������
    for (i = 0; i < sizeof(speed_arr) / sizeof(int); i++)
    {
        if (speed == name_arr[i])        //�жϴ������Ƿ����
        {
            tcflush(fd, TCIOFLUSH);             //ˢ�������������
            cfsetispeed(&Opt, speed_arr[i]);     //����ֱ�����
            cfsetospeed(&Opt, speed_arr[i]);

            // Opt.c_cflag |= (CLOCAL | CREAD);

            status = tcsetattr(fd, TCSANOW, &Opt);     //�������̰�bote rates��������д��������ȥ
            if (status != 0)
            {
                perror("uart set_speed err");         //���ô���
                return;
            }
            tcflush(fd, TCIOFLUSH);                    //ͬ��
            break;
        }
    }
}

//---------------------------------------------------------------
/**
*@brief   ���ô�������λ��ֹͣλ��Ч��λ
*@param  fd     ����  int  �򿪵Ĵ����ļ����
*@param  databits ����  int ����λ   ȡֵ Ϊ 7 ����8
*@param  stopbits ����  int ֹͣλ   ȡֵΪ 1 ����2
*@param  parity  ����  int  Ч������ ȡֵΪN,E,O,,S
*/

static    int     set_Parity(int fd, int databits, int stopbits, int parity)
{
    struct termios     options;     //����һ���ṹ

    if (tcgetattr(fd, &options) != 0)   //���ȶ�ȡϵͳĬ������options��,����
    {
        perror("uart set_Parity tcgetattr err");
        return(FALSE);
    }

    options.c_cflag &= ~CSIZE;         //��������c_cflagѡ���λ����λ����
    switch (databits) /*��������λ��*/
    {
    case 7:
        options.c_cflag |= CS7;     //����c_cflagѡ������λΪ7λ
        break;
    case 8:
        options.c_cflag |= CS8;     //����c_cflagѡ������λΪ8λ
        break;
    default:
        fprintf(stderr, "uart Unsupported data size\n");    //�����Ķ���֧��
        return (FALSE);
    }
    switch (parity) //������żУ�飬c_cflag��c_iflag��Ч
    {
    case 'n':
    case 'N':
        options.c_cflag &= ~PARENB;     /* Clear parity enable */
        options.c_iflag &= ~INPCK;         /* Enable parity checking */
        break;
    case 'o':     //��У�� ����PARENBУ��λ��Ч��PARODD��У��INPCK���У��
    case 'O':
        options.c_cflag |= (PARODD | PARENB);    /* ����Ϊ��Ч��*/
        options.c_iflag |= INPCK;                 /* Disnable parity checking */
        //options.c_iflag |=(INPCK|ISTRIP);
        break;
    case 'e':
    case 'E':     //żУ�飬��У�鲻ѡ����żУ����
        options.c_cflag |= PARENB;         /* Enable parity */
        options.c_cflag &= ~PARODD;     /* ת��ΪżЧ��*/
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
    if (parity != 'n')             //�������������Ƿ����У��
        options.c_iflag |= INPCK;
#endif

    /* ����ֹͣλ*/
    switch (stopbits)         //��������ֹͣλ����Ӱ��ı�־��c_cflag
    {
    case 1:
        options.c_cflag &= ~CSTOPB;     // ��ָ����ʾһλֹͣλ
        break;
    case 2:
        options.c_cflag |= CSTOPB;         //ָ��CSTOPB��ʾ��λ��ֻ�����ֿ���
        break;
    default:
        fprintf(stderr, "uart Unsupported stop bits\n");
        return (FALSE);
    }

//--------------------------------------------------------------------------------------------
//    tcflush(fd,TCIFLUSH);

    //RAW MODE
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);     /*Input*/    //����س�ֱ�����
    options.c_oflag  &= ~OPOST;                          /*Output*/

    //���ÿ����ַ��ͳ�ʱ�����ģ�һ��Ĭ�ϼ���
    options.c_cc[VTIME] = 150;         // 15 seconds
    options.c_cc[VMIN] = 0;            // ���ٶ�ȡ���ַ���

    options.c_iflag &= ~(IXON | IXOFF | IXANY);        //�����������

#if    1
    //�س��ͻ��в�����һ���ַ�
    options.c_oflag &= ~(INLCR | IGNCR | ICRNL);
    options.c_oflag &= ~(ONLCR | OCRNL);
#endif

    options.c_cflag |= (CLOCAL | CREAD);        //CLOCAL  : �������ӣ��޵��ƽ��������
    // CREAD   : �����������
//IGNPAR  : ignore bytes with parity errors
//ICRNL   : map CR to NL (otherwise a CR input on the other computer will not
//CRTSCTS : ���Ӳ������(ֻ���ھ�������·�������¹������ο� Serial-HOWTO ���߽�)
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
*@brief �򿪴���
*/
int        OpenUart(char    *dev)
{

    /*
    �����豸���ڶ�д�����ǲ�Ҫ�Կ��� tty ��ģʽ��
    ��Ϊ���ǲ���ϣ���ڷ��� Ctrl-C ������˽���
    */
// fd = open(dev, O_RDWR | O_NOCTTY );

    fd = open(dev, O_RDWR);                //| O_NOCTTY | O_NDELAY���ַ�ʽ��open����
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
    return    nread;    //nread �Ƿ�Ϊ0
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

    return 0 ;            //Ӳ��ǿ������DE �ߵ�ƽ

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

//���ȶ���uart_driver�ṹ������

/* static struct uart_driver clps711x_reg = {
 .driver_name  = "ttyCL",
 .dev_name  = "ttyCL",
 .major   = SERIAL_CLPS711X_MAJOR,//���豸�ţ��Զ�����߲������������define
 .minor   = SERIAL_CLPS711X_MINOR,//���豸��
 .nr   = UART_NR,//��������uart����һ���豸�ɶ�Ӧ���uart
 .cons   = CLPS711X_CONSOLE,//
};
*/
// ��Ҫ����ÿ���ڵĲ���������
// ÿ���ڵ�ops�Ǹ�ָ��ʵ�ʲ����ṹ��ָ�롣�䶨�����£��������Ҫ
//֮����Ҫע�ᵽϵͳ

/* ʣ�µľͲ���ops�ṹ��ĸ��������
���ʹ���жϣ���ôҪ��startup��ע���жϼ���Ӧ�������
���ڷ��ͽ��յ�ʵ�����ݣ���ʵ������Щ�鷳��
��Ҫ���͵����ݴ�����circ_buf�ṹ����Դ� uart_port->info->xmit;��õ���
xmit->buf[xmit->tail]ȡ�õ�ǰ�ַ���
if (uart_circ_empty(xmit)) ���ж��Ƿ������
���յ����ַ���Ҫ�͸�tty����ȥ����uart_port->info->tty���Եõ�tty��������ڡ�
tty_insert_flip_char(tty, ch, flg);
���Խ��ַ����뵽tty�У�����Ҫע�⣬tty�е�buff�����޵ģ�
��
if (tty->flip.count >= TTY_FLIPBUF_SIZE)���ж�
�����Ҫtty_flip_buffer_push(tty);

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


