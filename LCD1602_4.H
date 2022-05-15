/*--------------------------------------------------------------*/
//LCD1602四线驱动程序
//File:	LCD1602_4.H

/*--------------------------------------------------------------*/
#ifndef __LCD1602_4_H__		//防止被重复定义
#define __LCD1602_4_H__


/*--------------------------------------------------------------*/
//LCD1602接口定义
#define		LCD_DATA  P1		//P1口低四位与LCD的高四位(D4~D7)一一对应相接
sbit	LCD_RS		= P3^3;		//数据指令
sbit	LCD_EN		= P3^7;		//使能端



/*--------------------------------------------------------------*/
//函数声明
void LCD_init   (void);													//液晶初始化
void LCD_en     (void);													//输入使能
void LCD_clr	(void);													//液晶清屏
void LCD_cmd    (unsigned char cmd);									//指令输入
void LCD_dat    (unsigned char dat);									//数据输入
void LCD_delay  (unsigned char m, unsigned char n);						//延迟时间
void LCD_pos    (unsigned char x, unsigned char y);						//液晶定位
void LCD_printc (unsigned char x, unsigned char y, unsigned char c);	//字符定位输出
void LCD_prints (unsigned char x, unsigned char y, unsigned char *s);	//字符串定位输出

/*--------------------------------------------------------------*/
//延迟时间12us→130 000us
void LCD_delay(unsigned char m, unsigned char n)
{
    unsigned char tempm,tempn=n;
    do{tempm=m;while(--tempm);}while(--tempn);
}


/*--------------------------------------------------------------*/
//液晶初始化
void LCD_init(void)
{
	LCD_cmd(0x28);
	LCD_en();
	LCD_cmd(0x28);			//四线显示
	LCD_cmd(0x0c);			//显示打开
	LCD_cmd(0x01);			//显示清屏
	LCD_delay(2, 249);		//2000-2 us
	LCD_delay(2, 249);		//2000-2 us
	LCD_delay(2, 249);		//2000-2 us
}


/*--------------------------------------------------------------*/
//输入使能
void LCD_en(void)
{
	LCD_EN = 1;
	LCD_delay(1,1);			//12us
	LCD_delay(1,1);			//12us
	LCD_delay(1,1);			//12us
	LCD_EN = 0;
	LCD_delay(1,1);			//12us
	LCD_delay(1,1);			//12us
}


/*--------------------------------------------------------------*/
//指令输入
void LCD_cmd(unsigned char cmd)
{
	unsigned char i=0,j=0;
	
	LCD_delay(15,1);		//40us
	LCD_delay(15,1);		//40us
	LCD_delay(15,1);		//40us
	LCD_RS = 0;				//指令
	LCD_delay(15,1);		//40us
	LCD_delay(15,1);		//40us
	i=LCD_DATA&0xf0;
	
	j=cmd>>4;
	LCD_DATA = j|i;	//写高四位
	LCD_en();
	
	j=cmd&0x0f;				//低四位移到高四位
	LCD_DATA = j|i;
	LCD_en();
}


/*--------------------------------------------------------------*/
//数据输入
void LCD_dat(unsigned char dat)
{
	unsigned char i=0,j=0;
	
	LCD_delay(15,1);		//40us
	LCD_delay(15,1);		//40us
	LCD_delay(15,1);		//40us
	LCD_RS = 1;				//指令
	LCD_delay(15,1);		//40us
	LCD_delay(15,1);		//40us
	i=LCD_DATA&0xf0;
	
	j=dat>>4;
	LCD_DATA = j|i;	//写高四位
	LCD_en();
	
	j=dat&0x0f;				//低四位移到高四位
	LCD_DATA = j|i;
	LCD_en();
}


/*--------------------------------------------------------------*/
//液晶清屏
void LCD_clr(void)	
{
	LCD_cmd(0x01);
	LCD_delay(2, 249);		//2000-2 us
	LCD_delay(2, 249);		//2000-2 us
	LCD_delay(2, 249);		//2000-2 us
}


/*--------------------------------------------------------------*/
//液晶定位
void LCD_pos(unsigned char x, unsigned char y)
{
	if(y)	LCD_cmd(x|0xc0);
	else	LCD_cmd(x|0x80);
}



/*--------------------------------------------------------------*/
//字符定位输出
void LCD_printc(unsigned char x, unsigned char y, unsigned char c)
{
	LCD_pos(x, y);
	LCD_dat(c);
}


/*--------------------------------------------------------------*/
//字符串定位输出
void LCD_prints(unsigned char x, unsigned char y, unsigned char *s)
{
	LCD_pos(x, y);
	while(*s)
	{
		LCD_dat(*s);
		s++;
	}
}

#endif

 






