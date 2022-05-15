#include <stc11fxx.h>//ͷ�ļ�
//#include <intrins.h>
#include "LCD1602_4.H"
#include <string.h>
#define uchar unsigned char//�궨��
#define uint unsigned int//�궨��
#define u8 unsigned char
#define u16 unsigned short
#define U8 unsigned char
#define U16 unsigned short
#define TRUE 1U
#define FALSE 0U
#define ccts(c1, c2)    ((((c1) << 8) & 0xFF00) + ((c2) & 0x00FF))

typedef struct {
    u8          received;
    u8          waiting_head_byte;
    u8          pos;
    u8          packet_data[50];
    //TIMER_ID    terminal_timerid;
} reg_context_t;
reg_context_t reg_context = {0,0,0,{0}};

sbit  SCLK=P1^6;//TLC5615ʱ����
//sbit  CS=P1^5;//TLC5615Ƭѡ
//sbit  DIN=P1^7;//TLC5615������
sbit  CS=P1^7;//TLC5615Ƭѡ
sbit  DIN=P1^5;//TLC5615������


sbit  SW1=P1^4;

sbit  SIRQ=P3^2;


sbit  TXEN=P3^4;
sbit  SW2=P3^5;
sbit  RLED=P3^5;
//sbit  GLED=P3^4;


#define LCD_BUS P1
sbit RS=P3^3;
sbit EN=P3^7;

sbit stcf11_Rst_Pin = P3^6;

/*2022.5.14*/
//sbit Port_Pin_Test = P1^2 ;//panq,test for 1ms timer interrup

uint  S_dat;//���α���
bit   flag=0;//����OK��־
unsigned int TopData=0,LowData=0;

float base=0.0048828,TopVol=0,LowVol=0,PI=0,DO=0,RO=0;
unsigned char PI8H,PI8L,DO8H,DO8L;

#define     ClearLCD       0x01
#define     Return         0x02
#define     EntryMode      0x06
#define     DisplayOn      0x0c
#define     FunctionSet    0x28
#define     DisplayOff     0x08      //

/*******************************************************************************************************/
#define TEST_UART_STRING "PLC MCU Commuciation demo v0.01\r\n"
unsigned char T0RH = 0;     
unsigned char T0RL = 0;     
//unsigned char RxdByte = 0;  
unsigned int g_SoftTimerCnt = 0;
unsigned int g_SoftTimerEnable = 0;
u8 __local_reg_data[8] = {0x55,0xaa,0x12,0x34};

/*Note:time 2022.5.14,add Timer and Uart module*/
void ConfigTimer0(unsigned int ms);
void Uart_RT_Init(void);
//void System_Init(void);
void SendUartOneByte(unsigned char byte_to_send);
void SendUartMultiBytes(unsigned char *p_multiBytes,unsigned char len);
void delay_ms_for_modbus(unsigned int ms);
u8 reg_command_receive(u8 rdata);
/*
Function Prototype: void ConfigTimer0(unsigned int ms)
Entry Params: ms,the time how long to interrupt one timer .eg,configTimer0(5),5 ms invoke one Interrupt
Return Value:void
Author:
Time: 2022-5-14
description: config Timer0,enable timer 0 Interrupt
*/
unsigned short modbus_crc16_calc(unsigned char dat, unsigned short crc)
{
    unsigned char  i = 0;
    unsigned short check = 0;
    crc = crc ^ dat;

    for (i=0; i<8; i++)
    {
        check = crc & 1;
        crc = crc >> 1;
        crc = crc & 0x7FFF;

        if (1 == check)
        {
            crc = crc ^ 0xA001;
        }
        crc = crc & 0xFFFF;
    }

    return crc;
}

unsigned short modbus_crc16(const unsigned char *buf, unsigned int len)
{
    /*register */int counter;
    /*register */unsigned short crc = 0xFFFF;

    for (counter=0; counter<len; counter++)
    {
        crc = modbus_crc16_calc(buf[counter], crc);
    }

    return crc;
}

unsigned char modbus_crc16_check(unsigned char *buf, unsigned short buf_len)
{
    //u8  i;
    U8  crch;
    U8  crcl;
    U16 crc;
        
//    if (buf_len <= 4)
//    {
//        return 0;
//    }

    // 计算CRC校验
    crc = modbus_crc16(buf, buf_len - 2);
    crch = (U8)(crc >> 8);
    crcl = (U8)(crc);

//    if ((crcl != buf[buf_len - 2]) || (crch != buf[buf_len - 1]))
//    {
//        return 0;
//    }

    return 1;
}

u8 reg_modbus_crc_check(u8 *buf, u16 len)
{
    u8  crch;
    u8  crcl;
    u16 crc;

    if (len < 8)
    {
        return -1;
    }

    crc = modbus_crc16(buf, len - 2);
    crch = (U8)(crc >> 8);
    crcl = (U8)(crc);

//    if ((crcl != buf[len - 2]) || (crch != buf[len - 1]))
//    {
//         return -2;
//    }
    return 0;
}








void ConfigTimer0(unsigned int ms)
{
    unsigned long tmp;  //ÁÙÊ±±äÁ¿
    
    tmp = 11059200 / 12;      
    tmp = (tmp * ms) / 1000;  
    tmp = 65536 - tmp;        
    tmp = tmp + 18;           
    T0RH = (unsigned char)(tmp>>8);  
    T0RL = (unsigned char)tmp;
    TMOD &= 0xF0;   
    TMOD |= 0x01;   
    TH0 = T0RH;     
    TL0 = T0RL;
    ET0 = 1;        
    TR0 = 1;        
}

/*
Function Prototype: void Uart_RT_Init()
Entry Params: baud ,eg,9600,115200 etc,
Return Value:void
Author:
Time: 2022-5-14
description: config uart baudrate,enable uart interrupt
*/
void Uart_RT_Init(void)
{
  	//SCON = 0X50;
	SCON = 0xd0;
	TMOD &= 0X0F;
	TMOD |= 0X20;
	//TH1 = 220;
	//TL1 = 220;
	TH1 = 253;
	TL1 = 253;
	TR1 =1;
	ES = 1;
}

u8 reg_command_receive(u8 rdata)
{
 if (reg_context.waiting_head_byte == TRUE)
    {
        // 如果是接收到的第一个字节 则判断下是否是EDA的数据包头 不是则直接返回
//        if (rdata != 0x1)
//        //if (rdata != 0x01)
//        {
//            // 所有的EDA应答数据包都是由EDA_PACKET_HEAD开始 这样可以有效过滤到非法字符
//            return -1;
//        }

        reg_context.waiting_head_byte = FALSE;        
        reg_context.received = FALSE;

        reg_context.pos = 0;
        reg_context.packet_data[reg_context.pos++] = rdata;
			g_SoftTimerCnt = 0;
			g_SoftTimerEnable = 1;
        //timerStart(g_hTimer, reg_context.terminal_timerid, FALSE);
    }
    else
    {
        // 如果连续收到的数据超过了规定的最大长度 则忽略后续接收到的数据
//        if (reg_context.pos >= 100)
//        {
//			g_SoftTimerCnt = 0;
//			g_SoftTimerEnable = 1;
//            //timerStart(g_hTimer, reg_context.terminal_timerid, FALSE);
//            return 0;
//        }

        reg_context.packet_data[reg_context.pos++] = rdata;
        //timerStart(g_hTimer, reg_context.terminal_timerid, FALSE);
		g_SoftTimerCnt = 0;
		g_SoftTimerEnable = 1;
	}

    return 0;
}


/*isr for timer0,1ms enter this function one time*/
void InterruptTimer0() interrupt 1
{
    TH0 = T0RH;  //ÖØÐÂ¼ÓÔØÖØÔØÖµ
    TL0 = T0RL;

	if(	g_SoftTimerEnable == 1)
	{
		g_SoftTimerCnt++;
	}
	
//	if(g_SoftTimerCnt >= 0xFFFFFFFF) 
//	{
//		g_SoftTimerCnt = 0;
//	}
	
	//Port_Pin_Test = !Port_Pin_Test;
}

/*uart interrupt,when recevie a char from uart instrument,enter this function*/
void Interrupt_Uart() interrupt 4
{
	u8 uch;
   /*if(TI == 1)
   {
      TI = 0;
	  REN = 1;
   }*/
   if(RI == 1)
   {
   	  RI = 0;
	  uch = SBUF;
	  if(RB8 = P)
      {

      }
#if 1
	reg_command_receive(uch);
#endif
	  //SBUF =  RxdByte;
	  //REN = 0;  
   }


}

/*system initial,like timer,uart module,global interrupt enable */
//void System_Init(void)
//{
//	EA = 1;//opem global interrupt enable
////	Port_Pin_Test = 0;
//	ConfigTimer0(1);
//	Uart_RT_Init();//baud rate 9600
//}

/*send one byte by uart*/
void SendUartOneByte(unsigned char byte_to_send)
{
	TB8 = P;
	SBUF = byte_to_send;
	while(0 == TI);
	TI = 0;
}

void SendUartMultiBytes(unsigned char *p_multiBytes,unsigned char len)
{
	unsigned char i;
//	if(((void *)p_multiBytes == (void*)0) || (len == 0))
//	{
//		return;
//	}
	for(i=0;i<len;i++)
	{
		SendUartOneByte(p_multiBytes[i]);
	}
}

void delay_ms_for_modbus(unsigned int ms)
{
	unsigned int tmp = ms*93;
//	if(ms <= 0) return;
	while(tmp--);
}

/*********************************************************************************************************/
//void Delay1500ms(void)		//@11.0592MHz
//{
//	unsigned char i, j, k;

//	i = 64;
//	j = 9;
//	k = 179;
//	do
//	{
//		do
//		{
//			while (--k);
//		} while (--j);
//	} while (--i);
//}

//void Delay1000ms()		//@11.0592MHz
//{
//	unsigned char i, j, k;

//	i = 43;
//	j = 6;
//	k = 203;
//	do
//	{
//		do
//		{
//			while (--k);
//		} while (--j);
//	} while (--i);
//}


///*****************************************************/
///*void delay(uint x)//�ӳٺ���
//{
//  uint a,b;
//  for(a=0;a<50;a++)
//  for(b=x;b>0;b--);
//  } */
//  
///*****************************************************/  
//void Delay500us()		//@11.0592MHz
//{
//	unsigned char i, j;
//	i = 6;j = 93;
//	do{	while (--j);} 
//	while (--i);
//}

//void Delay10ms()		//@11.0592MHz
//{
//	unsigned char i, j;

//	_nop_();
//	_nop_();
//	i = 108;
//	j = 144;
//	do
//	{
//		while (--j);
//	} while (--i);
//}

//void Delay2ms()		//@11.0592MHz
//{
//	unsigned char i, j;

//	i = 22;
//	j = 128;
//	do
//	{
//		while (--j);
//	} while (--i);
//}

//void Delay200ms()		//@11.0592MHz
//{
//	unsigned char i, j, k;

//	i = 9;
//	j = 104;
//	k = 139;
//	do
//	{
//		do
//		{
//			while (--k);
//		} while (--j);
//	} while (--i);
//}

//void Delay2000ms()		//@11.0592MHz
//{
//	unsigned char i, j, k;

//	i = 85;
//	j = 12;
//	k = 155;
//	do
//	{
//		do
//		{
//			while (--k);
//		} while (--j);
//	} while (--i);
//}

//void Delay100ms()		//@11.0592MHz
//{
//	unsigned char i, j, k;

//	i = 5;
//	j = 52;
//	k = 195;
//	do
//	{
//		do
//		{
//			while (--k);
//		} while (--j);
//	} while (--i);
//}

//void Delay1ms()		//@11.0592MHz
//{
//	unsigned char i, j;

//	_nop_();
//	i = 11;
//	j = 190;
//	do
//	{
//		while (--j);
//	} while (--i);
//}


/*****************************************************/
void DAConvert(uint Data) //DACת������
{ 
  uchar i; 
  Data<<=6;//��������6λ,�ұ߶��� 
  SCLK=0; 
  CS=0; 
  for (i=0;i<12;i++)//
  { 
    if(Data&0x8000) 
	  DIN=1; 
    else DIN=0; 
    SCLK=1;  
    Data<<=1; 
    SCLK=0; 
  }  
  CS=1; 
} 

//void delayms(uint ms)		 //��ʱxx����
//{
// 	uchar i;
//	while(ms--)
//	{
//	 	for(i=0;i<120;i++);
//	}
//}

void EX_INT0() interrupt 0
{
   flag=1;
}

//void SendOut(void)
//{
//	TXEN=1;					//485���뷢��ģʽ
//	//SM2=0;					
//	SBUF=PI8H;	//װ��
//	while(TI!=1);TI=0;	//�ȴ����ڷ������
//	delayms(10);
//	SBUF=PI8L;//װ��
//	while(TI!=1);TI=0;	//�ȴ����ڷ������
//	delayms(10);
//	SBUF=DO8H;	//װ��
//	while(TI!=1);TI=0;	//�ȴ����ڷ������
//	delayms(10);
//	SBUF=DO8L;//װ��
//	while(TI!=1);TI=0;	//�ȴ����ڷ������
//	delayms(10);
//		
//}



/*****************************************************/
void main(void)
{
	unsigned int i=0,temp=0,tempA=0,tempB=0,j=0;
	u16 reg_addr;
	u16 reg_num;
	u16 crc;

	stcf11_Rst_Pin = 0;
#if 0
	//IAP_CONTR = 0x60;
	AUXR=0x00;
	TMOD=0x20;			//���ö�ʱ��1Ϊ������ʽ2���Զ�����8λ����
	TL1=0xfd;			//��ʱ��1װ�س�ʼֵ
	TH1=0xfd;
	PCON=0x00;			//SMOD=0; ���ڷ�ʽ1��2��3ʱ������������	
	TR1=1;				//������ʱ��1  
	SCON=0x50;			//���пڿ��ƼĴ���
#endif
	//System_Init();//2022.5.14
	EA = 1;//opem global interrupt enable
	//	Port_Pin_Test = 0;
	ConfigTimer0(1);
	Uart_RT_Init();//baud rate 9600
	//SendUartOneByte('a');
	//SendUartMultiBytes(TEST_UART_STRING,sizeof(TEST_UART_STRING));
#if 1
	//EA=1;					//�����ж�
	TXEN=1;				//485���뷢��ģʽ
	CLK_DIV=0x00;
	RLED=1;//GLED=1;
	SW1=1;SW2=1;
	//delayms(100);
	delay_ms_for_modbus(100);


   LCD_init();
	LCD_clr();
	LCD_prints(0,0,"PI:00.0 P-D:  . ");
	LCD_prints(0,1,"DO:00.0 D/P: 0% ");
	LCD_printc(15,0,' ');
	DAConvert(0);
  PI8H=0xaa,PI8L=0xaa;
  DO8H=0x55,DO8L=0x55;
#endif
	while(1)
	{
#if 1
	//delay_ms_for_modbus(1000);
	//stcf11_Rst_Pin = !stcf11_Rst_Pin;
	//Port_Pin_Test = !Port_Pin_Test;	
	//SendUartOneByte('b');
	//SendUartMultiBytes(TEST_UART_STRING,sizeof(TEST_UART_STRING));
		if(10 <= g_SoftTimerCnt)
		{
			reg_context.waiting_head_byte == TRUE;
			g_SoftTimerEnable = 0;
			g_SoftTimerCnt = 0;
			
/***************************************************************************************************************/
/*unpack modbus adu*/		
			reg_modbus_crc_check(&reg_context.packet_data[0],reg_context.pos);
//			if(reg_context.packet_data[0] != 0x1)
//			{
//				return ;
//			}

			switch(reg_context.packet_data[1])
			{
				case 0x4:/*read input registers*/
				{
					reg_addr = ccts(reg_context.packet_data[2],reg_context.packet_data[3]);
					reg_num = ccts(reg_context.packet_data[4],reg_context.packet_data[5]);

					//response
					reg_context.packet_data[0] = 0x1;	//0x01;
    				reg_context.packet_data[1] = 0x04;
   					 reg_context.packet_data[2] = 2*reg_num;
					for (i=0; i<reg_num; i++)
					{
						reg_context.packet_data[3+2*i]   = (u8)(__local_reg_data[i*2]);
						reg_context.packet_data[3+2*i+1] = (u8)(__local_reg_data[i*2+1]);
					}
					crc = modbus_crc16(reg_context.packet_data, 3+2*reg_num);
					reg_context.packet_data[3+2*reg_num]   = (u8)(crc);
					reg_context.packet_data[3+2*reg_num+1]   = (u8)(crc>>8);
					SendUartMultiBytes(&reg_context.packet_data[0],5+2*reg_num);
					reg_context.pos = 0;
					//reg_context.received = FALSE;
				}
				break;

				default:
				break;
			}
			
/**************************************************************************************************************/

/**************************************************************************************************************/
/*response to PLC*/
			//g_modbus_receive_buf_PacketTail_notify = 0x0;
			SendUartMultiBytes(&reg_context.packet_data[0],reg_context.pos);
			memset(&reg_context.packet_data[0],0x0,reg_context.pos);
			reg_context.pos = 0;
			//SendUartMultiBytes(g_modbus_receive_buf_T,g_modbus_receive_buf_PacketTail_pos);
			//memset(g_modbus_receive_buf_T,0x0,200);
			//g_modbus_receive_buf_PacketTail_pos = 0;
/**************************************************************************************************************/
		}
#endif
		//SendOut();delayms(100);
#if 1
		SW1=1;
		if(SW1==0)
		{
			//RLED=1;GLED=1;
			DAConvert(0);
			LCD_prints(0,0,"PI:00.0 P-D:  . ");
			LCD_prints(0,1,"DO:00.0 D/P: 0% ");
			//Delay10ms();
			delay_ms_for_modbus(10);
			DAConvert(1023);
			//for(j=0;j<20;j++)	Delay100ms();	//////////B�Σ����5V��ʱ��100ms*20
			for(j=0;j<20;j++)	delay_ms_for_modbus(100);
			DAConvert(0);
			//for(j=0;j<20;j++)	Delay100ms();  //////////C�Σ����0V��ʱ��100ms*20
			for(j=0;j<20;j++)	delay_ms_for_modbus(100); 
			if(SIRQ==0)
			{
				LCD_prints(3,0,">S< ");
			}
			else
			{
				for(i=0;i<1024;i++)	//D�����
				{
					DAConvert(i);
					//TopData=i;
					//Delay500us();
					//Delay500us();
					delay_ms_for_modbus(1);
					if(SIRQ==0)
					{
						TopData=i;
						break;
					}
				}
				
				
				if(SIRQ==1)	
				{	
					//RLED=0;
					DAConvert(0);
					LCD_prints(3,0,">W< ");
				}
				else
				{
					//for(j=0;j<10;j++)	Delay1ms();		//E�Σ���ʱ2ms
					for(j=0;j<10;j++)	delay_ms_for_modbus(1);	
					DAConvert(TopData+102);//DAConvert(TopData+0.5/base);��0.5V
					//for(j=0;j<10;j++)	Delay1ms();	//F�Σ���ʱ10ms
					for(j=0;j<10;j++)	delay_ms_for_modbus(1);	
					TopVol=(TopData)*base;
					PI=TopVol/0.005;
					//PI=12.174*PI-27.53;
					tempB=(unsigned int)PI;
					PI8H=tempB/256;
					PI8L=tempB%256;
					LCD_printc(3,0,tempB/100+0x30);
					LCD_printc(4,0,(tempB%100)/10+0x30);
					LCD_printc(6,0,tempB%10+0x30);
					
					//SW2=1;
					//while(SW1==0);
					//while(SW2==1);
					
	  
					for(i=(TopData+102);i>0;i--)	//��0.5V�� G��
					{
						DAConvert(i);
						//LowData=i;
						//Delay500us();
						delay_ms_for_modbus(1);
						//Delay500us();
						if(SIRQ==1)
						{
							LowData=i;
							break;
						}
					}
					LowVol=(LowData)*base;
					DO=LowVol/0.005;
					//DO=12.578*DO-39;
					tempA=(unsigned int)DO;
					DO8H=tempA/256;
					DO8L=tempA%256;
					LCD_printc(3,1,tempA/100+0x30);
					LCD_printc(4,1,(tempA%100)/10+0x30);
					LCD_printc(6,1,tempA%10+0x30);
					DAConvert(0);
//					SendOut();
					RLED=0;

					RO=(DO/PI);
					temp=(unsigned int)((RO*100)+0.5);
					LCD_printc(13,1,temp/10+0x30);
					LCD_printc(14,1,temp%10+0x30);
					//GLED=0;
					//DAConvert(0);
					
					temp=tempB-tempA;
					LCD_printc(12,0,temp/100+0x30);
					LCD_printc(13,0,(temp%100)/10+0x30);
					LCD_printc(15,0,temp%10+0x30);
					RLED=0;
					//DAConvert(TopData);
					//Delay1000ms();
					delay_ms_for_modbus(100);
					//DAConvert(0);
					RLED=1;
					//while(SW2==0);
				}
			}
		}
#endif
		
	}
  
}


/*****************************************************/
/*void main(void)
{
	unsigned int i=0,temp=0;
	CLK_DIV=0x00;
	RLED=1;GLED=1;SW=1;
	delayms(100);
	//IT0 = 1;
	//EA=1; //�����ж�  
	//EX0=0;

   LCD_init();
	LCD_clr();
	LCD_prints(0,0,"PI:00.0         ");
	LCD_prints(0,1,"DO:00.0   Rx: 0%");
	LCD_printc(15,0,' ');
	DAConvert(0);
  
	while(1)
	{
		SW=1;
		if(SW==0)
		{
			RLED=1;GLED=1;
			LCD_prints(0,0,"PI:00.0         ");
			LCD_prints(0,1,"DO:00.0   Rx: 0%");
			
			for(i=0;i<1024;i++)
			{
				i=330;
				DAConvert(i);
				TopData=i;
				Delay500us();
				if(SIRQ==0)
				{
					break;
				}
			}
			if(SIRQ==1)	
			{	
				RLED=0;DAConvert(0);
			}
			else
			{
				TopVol=(TopData)*base;
				PI=TopVol/0.005;
				temp=(unsigned int)PI;
				LCD_printc(3,0,temp/100+0x30);
				LCD_printc(4,0,(temp%100)/10+0x30);
				LCD_printc(6,0,temp%10+0x30);
			}
		}
		
  }
  
}*/


