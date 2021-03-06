#include "bsp_cc1101.h"
#include  "./led/bsp_led.h"
#include "bsp_iwdg.h"

/************************************************************************************************************
**	说明：命令cmd:(一个字节) 有一个报头位,R(1)W(0), 突发位(B:1为突发位，即连续读取或写入，且地址要求低于0x2F),地址位
**				 A7   		A6   	A5   A4    A3    A2    A1    A0
**				 R/W	  	B		A5-A0: 地址位
**				当地址位大于0x2F时，突发位将不再表示突发
**
**	 特别注意:	单字节发送时
**					时钟频率小于等于9MHz时,在发送地址和数据之间不需要延时
**			  		如果时钟频率大于9MHz时,在发送地址和发送数据之间需要添加延时
**					具体时钟频率是多少以及SPI通信的最大速率,请参考1101数据手册
**				连续发送时
**					时钟频率小于等于6.5MHz时,在发送地址和数据之间不需要延时
**				
*************************************************************************************************************/



/****************************************************************************************
**	描	述：只写命令写入函数,对于选通指令(13个),只需要发送一次指令即可,不需要发送数据(单字节指令)
**			发送选通指令会使内部状态或模式发生改变
**			注:当在一个函数中使用两次此函数时,在某种情况下和函数Write_Data()相当
**	参	数：CMD:选通指令,CC1101头文件查找
**	返回值：无
****************************************************************************************/
void Write_CMD(uint8_t CMD)
{
	SPI_CS_LOW();						/*拉低片选*/
	while((GPIO_ReadInputDataBit(SPI_GPIO_PORT, SPI_MISO_PIN)& 0x80) == Bit_SET); //等待MISO引脚为低
	SPI_Send(CMD);						/*发送指令,读寄存器的地址*/
	SPI_CS_HIGH();						/*拉高片选*/
}


/****************************************************************************************
**	描	述：写数据函数,两次发送,第一次发送指令,第二次发送数据
**	参	数：Write_Addr:要写入的地址,CC1101头文件查找
**			Write_data:要写入的数据
**	返回值：无
****************************************************************************************/
void Write_Data(uint8_t Write_Addr,uint8_t Write_data)
{
	SPI_CS_LOW();							/* 拉低片选 */
	while((GPIO_ReadInputDataBit(SPI_GPIO_PORT, SPI_MISO_PIN)& 0x80) == Bit_SET); //等待MISO引脚为低
	SPI_Send(Write_Addr);					/*发送指令,读寄存器的地址*/
	SPI_Send(Write_data);					/* 发送数据 */
	SPI_CS_HIGH();							/*拉高片选*/
}


/************************************************************************************************
**	描	述：CC1101初始化,配置模式: 数据速率:250kBaud,调试方式:GFSK,频带:868MHz
**	参	数：
**	返回值：无
*************************************************************************************************/
void CC1101_Init(void)  
{      
    //Date rate:250kBaud,Dev:127kkHz, Mod:GFSK, RX BW:540kHz,,base frequency:433MHz,optimized for current consumption
	Write_Data(IOCFG2_ADDR,0x29);      //配置GDIO2为默认状态CHIP_RDYn
	Write_Data(IOCFG0_ADDR,0x01);       //在接收到数据包末尾时置位,RX FIFO为空时,取消置位,对于接收模块配置成0x01,如果是发送模块配置成0x06表示发送置位  
	  
	//配置为433MHz
	Write_Data(FREQ2,0x10);             //频率控制词汇，高字节  
	Write_Data(FREQ1,0xA7);             //频率控制词汇，中间字节  
	Write_Data(FREQ0,0x62);             //频率控制词汇，低字节  
	  
	//250kbaud  
	Write_Data(MDMCFG4,0x2D);           //调制器配置  
	Write_Data(MDMCFG3,0x3B);           //调制器配置   
	Write_Data(MDMCFG2,0x13);           //调制器配置

	Write_Data(DEVIATN,0x62);           //调制器设置。选配  
	Write_Data(MCSM0,0x18);             //主通信控制状态机配置  
	Write_Data(FOCCFG,0x1D);            //频率偏移补偿配置  
 
	Write_Data(FSCAL3,0xEA);            //频率合成器校准  
	Write_Data(FSCAL2,0x2A);            //频率合成器校准  
	Write_Data(FSCAL1,0x00);            //频率合成器校准  
	Write_Data(FSCAL0,0x1F);            //频率合成器校准 


	//地址匹配
	//开启地址滤波
	Write_Data(PKTCTRL1,0x01);            
	Write_Data(ADDR,CC1101_UNIQUE_ADDR);
}
	

/************************************************************************************************
**	描	述：写入数据到发送缓冲区(多字节数据) 
**	参	数：pBuffer:要写入的数据,len:写入数据的长度
**	返回值：无
*************************************************************************************************/

void WriteTxFITO(uint8_t * pBuffer,uint8_t len)
{
	Write_Data(WRITE_BURST_FIFO,len);
	Write_burst(WRITE_BURST_FIFO,pBuffer,len);
}



/************************************************************************************************
**	描	述：读取接收缓冲区的数据
**	参	数：pBuffer:需要读取的数据,len:读取数据的长度
**	返回值：无
*************************************************************************************************/
void ReadRxFIFO(uint8_t *data,uint16_t len)
{
	Read_burst(READ_BURST_FIFO,data,len);
}


/************************************************************************************************
**	描	述：发送数据,将缓冲区数据全部发送出去(多数据) 【此函数还需测试】
**	参	数：pBuffer:需要读取的数据,len:读取数据的长度
**	返回值：无
*************************************************************************************************/
 void CC1101_RFDataPack_Send(uint8_t *pBuff, uint16_t len)  
{  
    Write_CMD(SIDLE);   			//进入空闲模式
    Write_CMD(SFTX);    			//清空发送缓冲区,只能在IDLE状态下清空  
    WriteTxFITO(pBuff, len);     	//写入数据到发送缓冲区
    Write_CMD(STX);     			//开始发送数据  
	
	Write_CMD(SFTX);			//刷新发送缓存区
    Write_CMD(SIDLE);   //退出当前模式  
} 


/************************************************************************************************
**	描	述：接收数据,将接收缓冲区数据全部接收
**	参	数：pBuffer:需要接收的数据,len:接收数据的长度
**	返回值：无
*************************************************************************************************/
void CC1101_RFDataPack_Rceive(uint8_t *pBuff, uint16_t len)
{
	//说明:SFTX或SFRX 指令选通脉冲仅在IDLE、TXFIFO_UNDERFLOW 或 RXFIFO_OVERFLOW 状态下才能发出
	Write_CMD(SFRX);				//在空闲状态里清空数据
//	Write_CMD(SRX);   				//进入接收模式,在初始化配置中,已经默认配置为接收完成后进入空闲状态	
	while(GPIO_ReadInputDataBit(IOCFG0_GPIO_PORT, IOCFG0_GPIO_PIN) != SET) 	//如果GDIO0还未置位,条件为真,空等
	{
		//已知bug:长时间没有接收数据，有一定的几率出现无法接收数据,只能通过复位MCU来实现
		//上面bug原因已找到: CC1101定时器终止了RX状态
		Write_CMD(SRX);	//为防止定时器终止CC1101的RX状态,在此循环里强制使其进入RX状态
		
	}
	while(GPIO_ReadInputDataBit(IOCFG0_GPIO_PORT, IOCFG0_GPIO_PIN) == SET) //等待GDIO0置位,置位表示已经接收到数据包末尾
	{	
		ReadRxFIFO(pBuff,len);     		//接收数据,数据读取
		Write_CMD(SFRX);				//此行代码用于解决出现卡死的bug
	}
}	


/************************************************************************************************
**	描	述：连续发送数据
**	参	数：Write_Addr:要写入数据的地址
**			pBuffer   :写入的数据
**			Length	  :写入数据的长度
**	返回值：无
*************************************************************************************************/
void Write_burst(uint8_t Write_Addr,uint8_t *pbuffer,uint8_t Length)
{
	uint8_t i = 0;
	SPI_CS_LOW();							//拉低片选
	while((GPIO_ReadInputDataBit(SPI_GPIO_PORT, SPI_MISO_PIN)& 0x80) == Bit_SET); //等待MISO引脚为低
	Delay_us(20);							//延时
	SPI_Send(Write_Addr);					//发送要写的寄存器的地址,突发位必须为1
	for(i=0; i < Length;i++)
	{
		SPI_Send(pbuffer[i]);				//循环发送数据
	}
	SPI_CS_HIGH();							//拉高片选,结束连续发送,连续发送时,必须通过拉高片选结束发送
}


/************************************************************************************************
**	描	述：连续读取数据
**	参	数：Read_Addr :要读取数据的地址
**			pBuffer   :将读取的数据存入到pBuffer中
**			Length	  :读取数据的长度
**	返回值：无
*************************************************************************************************/
void Read_burst(uint8_t Read_Addr,uint8_t *data,uint8_t Length)
{
	uint8_t i = 0;
	SPI_CS_LOW();							//拉低片选
	while((GPIO_ReadInputDataBit(SPI_GPIO_PORT, SPI_MISO_PIN)& 0x80) == Bit_SET); //等待MISO引脚为低
	SPI_Send(Read_Addr);
	for(i=0; i < Length;i++)
	{
		data[i] = SPI_Send(0);				//连续读取数据,经测试,把0改成READ_BURST也没问题
	}
	SPI_CS_HIGH();							//拉高片选
}


/************************************************************************************************
**	描	述：CC1101/CC115L系统复位函数
**	参	数：无
**	返回值：无
*************************************************************************************************/
void CC1101_Reset(void)
{
	Write_CMD(SRES);
}


/************************************************************************************************
**	描	述：CRC校验
**	参	数：data	:需要校验的数据
**			length	:校验数据的长度
**	返回值：CRC校验结果
**************************************************************************************************/
 uint8_t Crc_Calcu(uint8_t * data,uint8_t length)
 {
	uint8_t crc = 0;
	uint8_t i = 2;
	for(i = 2;i < length; i++)
	{
		crc += data[i];
	}
	crc ^= 0xFF;
	return crc;
 }
