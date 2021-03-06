/*
  *******************************************************************************
  * @file    meter_table.c
  * @author  zjjin
  * @version V0.0.0
  * @date    03-08-2016
  * @brief
  ******************************************************************************
  * @attention
  *各种表的配置都可以写在这个文件中。
  *
  ******************************************************************************
*/

#include "includes.h"
#include "meter_table.h"
#include "bsp.h"

//抄表串口， 函数指针赋值
uint8 (*METER_ComParaSetArray[])(void) = {METER_ComSet1, METER_ComSet2, METER_ComSet3, METER_ComSet4,METER_ComSet5,METER_ComSet6,METER_ComSet7};

//热计量表多协议支持，通过该Table来区分不同热计量表设置及抄读等
// 对于不同协议的读取热计量表数据的标识符不同，
// Table 格式为：串口设置函数指针标号 + 读取热计量表数据标识符 + 前导符个数+通讯类型(0:MBUS  1:485)
uint16 gMETER_Table[HEATMETER_PROTO_SUM][4] = {
		{COMSET_1, 0x901F, 4,  0},			//德鲁MBUS总线热表------ 0
		{COMSET_2, 0x52C3, 4,  0},			//天津万华------   1	硬件协议不兼容海威茨 -林晓彬
		{COMSET_1, 0x1F90, 0,  0},			//丹佛森---------  2
		{COMSET_1, 0x1F90, 4,  0},			//立创协议1--------3	  威海震宇1表,关键是表的厂家代码一样 ，经纬不息屏版 ,注：力创有两种表，数据解析的时候小数点未知不同
		{COMSET_3, 0x1F90, 4,  1},			//乐业485--------- 4
			{COMSET_4, 0x901F, 4,  0},           //天津万华 大表--- 5，万华的小表也用
			{COMSET_1, 0x1F90, 4,  0},		     //HYDROMETER------ 6
			{COMSET_1, 0x1F90, 4,  0},			//ZENNER真兰----   7
			{COMSET_1, 0x1F90, 4,  0},			//LANDISGYR 兰吉尔---  8
			{COMSET_1, 0x1F90, 4,  0},			//engelmann恩乐曼- 9
			{0,0,0,0},                           //10
			{0,0,0,0},                           //11
			{COMSET_5, 0x0,    0,  5},           //12 亿林阀门
			{COMSET_1, 0x1F90, 2,  0},           /*13热分配*/
			{0,0,0,0},                           //14
			{0,0,0,0},                           //15

		//begin: 协议添加，by zjjin.
		{COMSET_1, 0x901F, 4,  0}, 	//新天	          16   2400，一个停止位	，偶校验
		{COMSET_1, 0x1F90, 4,  0}, 	//暖流	          17   2400波特率，一个起始位，8个数据位，1个校验位，一个停止位	，偶校验
		{COMSET_1, 0x1F90, 4,  0}, 	//鸿翔	          18	波特率无
		{COMSET_6, 0x1F90, 4,  1}, 	//联强485  嘉洁能485   19    1200波特率，一个起始位，8个数据位，1个校验位，一个停止位	，偶校验
		{COMSET_1, 0x901F, 0,  0}, 	//海威茨	      20	 2400波特率，一个起始位，8个数据位，1个校验位，一个停止位	，偶校验
		{COMSET_1, 0x1f90, 13, 0},		//开元	      21	2400波特率，一个起始位，8个数据位，1个校验位，一个停止位	，偶校验
		{COMSET_1, 0x1F90, 3,  0}, 	//经纬	  22	2400波特率，一个起始位，8个数据位，1个校验位，一个停止位	，偶校验
		{COMSET_1, 0x1F90, 4,  0}, 	//联强MBUS	  23
		{COMSET_1, 0x1F90, 4,  0}, 	//力创协议2    24力创两种表数据解析的时候，本协议目前仅用于日照安泰易居小区
		{COMSET_1, 0x901F, 4,  0},		//积成热表， 25组帧格式不一样					//德鲁----- ------ 0
		{COMSET_1, 0x1F90, 4,  0},			//26威海震宇2表,关键是表的厂家代码一样 ，经纬不息屏版 ,注：力创有两种表，数据解析的时候小数点未知不同
		{COMSET_1, 0x1F90, 4,  0},			//27    经纬带标识FE版本协议
		{COMSET_1, 0x1F90, 3,  0},			//28    天津易通达
		{COMSET_3, 0x901F, 4,  1},			//29    德鲁RS485总线热表
		{COMSET_2, 0x52C3, 4,  0},			//30    天津万华,按照德鲁协议解析。
		{COMSET_1, 0x1F90, 4,  0},			//31    威海震宇S型热量表多一组累计流量数据。
		{COMSET_1, 0x1F90, 3,  0},			//32    威海米特热表。
		{COMSET_1, 0x1F90, 4,  0},			//33    威海天罡欧标1协议(威海颐和小区).
		{COMSET_1, 0x901F, 4,  0},			//34    浙江超仪水表专用。
		{COMSET_1, 0x901F, 3,  0},			//35    威海米特21xx热表。
		{COMSET_1, 0x1F90, 4,  0},			//36    兰吉尔F4热量表。
		{COMSET_1, 0x1F90, 4,  0},			//37    兰吉尔T230热量表。
		{COMSET_1, 0x1F90, 4,  0}			//38	   贝特2014年及以前的大口径热表协议(日照公安局办公楼)
		//end: 协议添加，by zjjin.
};
