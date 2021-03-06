/*
  *******************************************************************************
  * @file    sysfuctions.c
  * @author  zjjin
  * @version V0.0.0
  * @date    02-24-2016
  * @brief
  ******************************************************************************
  * @attention
  *
  *
  ******************************************************************************
  */
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <libxml/xmlwriter.h>
#include <libxml/encoding.h>
#include <libxml/parser.h>
#include <libxml/tree.h>



#include "includes.h"
#include "sysfuctions.h"
#include "commap.h"
#include "rs485up.h"



#define DEFAULT_ENCODING "utf-8"


uint8 gDebugLevel = 1;
uint8 gDebugModule[20];
sem_t  CRITICAL_sem;
sem_t  GprsIPDTask_sem;//控制GprsIPD线程是否运行的信号量(暂没使用其他线程挂起的方法。)。
sem_t  Gprs_Sem;
sem_t  RS485Up_sem;  //上行485端口信号量
sem_t  RS485Down_sem;//下行485端口信号量
sem_t  GPRSPort_sem; //GPRS端口信号量
sem_t  OperateDB_sem; //操作数据库的信号量
sem_t  OperateMBUS_sem;	//操作MBUS端口信号量
sem_t  Opetate485Down_sem;	//操作485下行端口信号量。

sem_t Validate_sem;  //GPRS登录信号量。

sem_t His_up_sem;//历史数据上传信号量
sem_t His_asw_sem;//历史数据应答信号量

//base64映射表
static const char base64_code[] = \
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

void OSTimeDly(uint32 ms)
{
	usleep(ms*1000);
}


void OS_ENTER_CRITICAL(void)
{
	int ret,value=0;
	ret = sem_getvalue(&CRITICAL_sem,&value);
	if(ret)  {
		debug_err(gDebugModule[MSIC_MODULE],"[%s][%s][%d]value=%d \n",FILE_LINE,ret);
	}
	debug_debug(gDebugModule[MSIC_MODULE],"[%s][%s][%d]value=%d \n",FILE_LINE,value);

	sem_wait(&CRITICAL_sem);	/*减一*/
}


void OS_EXIT_CRITICAL(void)
{
	int ret=0,value=0;
	ret = sem_getvalue(&CRITICAL_sem,&value);
	if(ret){
		printf("[%s][%s][%d]value=%d \n",FILE_LINE,ret);
	}
	debug_debug(gDebugModule[MSIC_MODULE],"[%s][%s][%d]value=%d \n",FILE_LINE,value);

	sem_post(&CRITICAL_sem);  /*加一*/
}



void  OSSemPend (uint8 dev,uint32  timeout,uint8 *perr)
{
	int ret=0;
	struct timespec outtime;
	struct timeval now;

	gettimeofday(&now,NULL);

	outtime.tv_sec = now.tv_sec + timeout/1000;
	outtime.tv_nsec = now.tv_usec + (timeout%1000)*1000*1000;

	outtime.tv_sec += outtime.tv_nsec/(1000*1000*1000);
	outtime.tv_nsec %= (1000*1000*1000);
	//printf("[%s][%s][%d]dev= %d\n",FILE_LINE, dev);
	if(timeout == (uint32)NULL) {
		sem_wait(&QueueSems[dev]);
	}
	else{
		ret = sem_timedwait(&QueueSems[dev], &outtime);
		if(ret != 0) {
			//printf("[%s][%s][%d]dev= %d, OSSemPend err=%d\n",FILE_LINE, dev, ret);
			debug_err(gDebugModule[MSIC_MODULE],"[%s][%s][%d]sem_timedwait %s\n",FILE_LINE,strerror(errno));
		}
	}

	*perr = (uint8)ret;

}


void OSSemPost(uint8 dev)
{
	sem_post(&QueueSems[dev]);  /*加一*/
}


/*
  ******************************************************************************
  * 函数名称： int OSSem_timedwait (sem_t Sem,uint32  timeout)
  * 说    明： 可定时等待一个信号量的到来。
  * 参    数： 成功等待到信号量，返回0，非0说明超时。
			sem_t Sem 要等待的信号量。
			uint32  timeout 超时时间，单位毫秒。
  ******************************************************************************
*/

int OSSem_timedwait(sem_t *pSem,uint32  timeout)
{
	int ret=0;
	struct timespec outtime;
	struct timeval now;

	gettimeofday(&now,NULL);

	outtime.tv_sec = now.tv_sec + timeout/1000;
	outtime.tv_nsec = now.tv_usec + (timeout%1000)*1000*1000;

	outtime.tv_sec += outtime.tv_nsec/(1000*1000*1000);
	outtime.tv_nsec %= (1000*1000*1000);

	if(timeout == (uint32)NULL){
		sem_wait(pSem);
	}
	else{
			ret = sem_timedwait(pSem,&outtime);
		if(ret != 0){
			debug_err(gDebugModule[MSIC_MODULE],"[%s][%s][%d]sem_timedwait %s\n",FILE_LINE,strerror(errno));
			}
	}

	return ret;


}





/*
  ******************************************************************************
  * 函数名称： uint8 Str2Bin(char *str, uint8 *array,uint16 lens)
  * 说    明： 将字符串转换为十六进制数。
  * 参    数： lens为array指向空间的字节数，以防str较长，array空间不足带来的问题。
  ******************************************************************************
*/
uint8 Str2Bin(char *str, uint8 *array,uint16 lens)
{
	char temp;
	char data;
	uint16 lu16counter = 0;

	int number = 0;
	char tempLow, tempHigh;

	number = strlen(str);
	//printf("[%s][%s][%d] number = %d\n",FILE_LINE,number);
	if(number%2)
	{
		number++;
		data = *str++;
		tempHigh = 0x00;
		tempLow  = Ascii2Hex(data);
		//printf("templow data is %x\n", tempLow);
		*array++ = (tempHigh << 4) + tempLow;
		//printf("creat array data is %x\n", (tempHigh << 4) + tempLow);
		lu16counter += 1;
	}

	//printf("The string 2 Hex data is: ");
	while(*str != '\0')
	{
		if(lu16counter < lens){
			data = *str++;
			temp = Ascii2Hex(data)<<4;
			data = *str++;
			temp += Ascii2Hex(data);

			*array++ = temp;
			//printf(" %x ,", temp);

			lu16counter += 1;
		}
		else{
			break;
		}

	}
	//printf("\n");
	return 0;

}


/*
  ******************************************************************************
  * 函数名称： uint8 PUBLIC_CountCS(uint8* _data, uint16 _len)
  * 说    明： 计算一串数字的累加和校验，溢出部分舍弃。
  * 参    数：
  ******************************************************************************
*/

uint8 PUBLIC_CountCS(uint8* _data, uint16 _len)
{
	uint8 cs = 0;
	uint16 i;

	for(i=0;i<_len;i++)
	{
	   cs += *_data++;
	}

	return cs;
}

/*
******************************************************************************
* 函数名称：void FileSend(uint8 Dev, FILE *fp)
* 说	明： 发送一个文件内容。
* 参	数： Dev可以选择通过485还是GPRS发送。
******************************************************************************
*/
void FileSend(uint8 Dev, FILE *fp)
{
	switch(Dev){
		case UP_COMMU_DEV_AT:
		case UP_COMMU_DEV_GPRS:
			GPRS_FileSend(UP_COMMU_DEV_GPRS,fp);
			break;

		case UP_COMMU_DEV_485:
			RS485Up_FileSend(UP_COMMU_DEV_485,fp);
			break;

		default:
			break;


	}

}

//把asctime()函数返回的时间格式, 转换为"YYYY-MM-DD hh:mm:ss"
uint8 asc_to_datestr(char* src, char* dest)
{
	int i=0;
	char *pTimeStr[6]={NULL};
	char *buf=src;
	char *saveptr=NULL;
	char *monEng[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
	char *monDec[12]={"01","02","03","04","05","06","07","08","09","10","11","12"};
	char tmpstr[5];
	printf("[%s][%s][%d]src: %s\n", FILE_LINE, src);
	i=0;
	while((pTimeStr[i] = strtok_r(buf, " ", &saveptr)))
	{
		i++;
		buf=NULL;
	}

	strncpy(dest, pTimeStr[4], 4);//year
	strcat(dest, "-");
	for(i=0;i<12;i++)
		if(strcmp(pTimeStr[1], monEng[i])==0)
			strcat(dest, monDec[i]);//mon

	strcat(dest, "-");
	sprintf(tmpstr, "%02d", atoi(pTimeStr[2]));//10以下的日期, src是不补0的
	strcat(dest, tmpstr);//day
	strcat(dest, " ");
	strcat(dest, pTimeStr[3]);//time
	printf("[%s][%s][%d]dest: %s\n", FILE_LINE,  dest);
	return NO_ERR;
}

/* 计算ModBus-RTU传输协议的CRC校验值
 * nData, 指向消息头的指针;
 * wLength, 消息体的长度
 */
uint16 crc16ModRtu(const uint8 *nData, uint16 wLength)
{
	static const uint16 wCRCTable[] = {
		0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
		0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
		0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
		0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
		0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
		0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
		0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
		0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
		0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
		0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
		0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
		0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
		0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
		0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
		0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
		0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
		0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
		0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
		0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
		0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
		0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
		0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
		0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
		0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
		0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
		0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
		0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
		0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
		0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
		0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
		0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
		0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
	};

	uint8 nTemp;
	uint16 wCRCWord = 0xFFFF;

	while (wLength--) {
		nTemp = *nData++ ^ wCRCWord;
		wCRCWord >>= LEN_BYTE;
		wCRCWord  ^= wCRCTable[nTemp];
	}
	return wCRCWord;
}

int idx_of_base64(char b)
{
	if(b<='Z' && b>='A')
		return (b-'A');
	if(b<='z' && b>='a')
		return (b-'a'+26);//a偏移了26个字母
	if(b<='9' && b>='0')
		return (b-'0'+52);//0偏移了52个字母
	if(b == '+')
		return 62;
	if(b == '/')
		return 63;
	if(b == '=')
		return 64;
	return 65;//其他字符均返回65, 以与编码表内的字符区别
}

/*
 * 将base64编码过的字符串, 解码为相应的二进制串
 * enStr: base64编码过的字符串
 * enSize: 编码过的字符串长度
 * deStr: 解码出的二进制串
 */
uint8 decode_base64(char* enStr, int enSize, uint8* deStr)
{
	if( (enSize%4) != 0)//base64编码过的字串必须为4的整数倍
		return ERR_1;
	uint8 b[4];
	int i;
	for(i=0; i<enSize; i+=4) {
		b[0] = idx_of_base64(enStr[i]);
		b[1] = idx_of_base64(enStr[i+1]);
		b[2] = idx_of_base64(enStr[i+2]);
		b[3] = idx_of_base64(enStr[i+3]);
		*deStr++ = (b[0]<<2|b[1]>>4);
		if (b[2] < 64) {
			*deStr++ = ((b[1] << 4) | (b[2] >> 2));
			if (b[3] < 64) {
				*deStr++ = ((b[2] << 6) | b[3]);
			}
		}
	}
	return NO_ERR;
}

//计数base64编码当中符号'='出现的次数
int cnt_of_pad(char* enStr, int enSize)
{
	int cnt=0;
	int i;
	for(i=0;i<2;i++) {
		if(enStr[enSize-1-i] == '=')
			cnt++;
	}
	return cnt;
}

//将s指向的, 长度为len的字节数组, 编码为base64字符串
uint8 encode_base64(char* s, int len, char* enStr)
{
	if(len == 0)
		return ERR_1;

	int idx;
	int i=0;
	for(i=0;i<len;i += 3) {
		idx = (s[i] & 0xFC) >> 2;
		*enStr++ = base64_code[idx];

		idx = ((s[i] & 0x03) << 4);
		if (i+1 < len) {
			idx |= ((s[i+1] & 0xF0) >> 4);
			*enStr++ = base64_code[idx];
			idx = ((s[i+1] & 0x0F) << 2);
			if (i+2 < len) {
				idx |= ((s[i+2] & 0xC0) >> 6);
				*enStr++ = base64_code[idx];
				idx = (s[i+2] & 0x3F);
				*enStr++ = base64_code[idx];
			} else {
				*enStr++ = base64_code[idx];
				*enStr++ = '=';
			}
		} else {
			*enStr++ = base64_code[idx];
			*enStr++ = '=';
			*enStr++ = '=';
		}
	}
	return NO_ERR;
}

//生成用于测试的升级文件帧
void gen_file_frame(char* filename, char* gateway_id, char* server_id)
{
	xmlTextWriterPtr writer;
	xmlDocPtr doc;
	char file[20];
	char log[1024];
	FILE *fp;
	char file_buf[1024]={0};
	int j, len;
	char tmpstr[50];
	uint16 crc;

	if ((fp = fopen(filename, "rb")) == NULL) {
		sprintf(log, "[%s][%s][%d]open file err\n", FILE_LINE);
		write_log_file(log,strlen(log));
		return;
	}
	j=1;
	while((len=fread(file_buf, 1, 1024, fp ))) {
		writer = xmlNewTextWriterDoc(&doc, 0);
		xmlTextWriterStartDocument(writer, NULL, DEFAULT_ENCODING, NULL);
		xmlTextWriterStartElement(writer, BAD_CAST "root");
		//start common
		xmlTextWriterStartElement(writer, BAD_CAST "common");

		//gateway_id
		xmlTextWriterStartElement(writer, BAD_CAST "sadd");
		xmlTextWriterWriteString(writer, BAD_CAST server_id);
		xmlTextWriterEndElement(writer);
		//sever_id
		xmlTextWriterStartElement(writer, BAD_CAST "oadd");
		xmlTextWriterWriteString(writer, BAD_CAST gateway_id);
		xmlTextWriterEndElement(writer);
		//func_id
		xmlTextWriterStartElement(writer, BAD_CAST "func_type");
		xmlTextWriterWriteString(writer, BAD_CAST "11");
		xmlTextWriterEndElement(writer);
		//operation_id
		xmlTextWriterStartElement(writer, BAD_CAST "oper_type");
		xmlTextWriterWriteString(writer, BAD_CAST "1");
		xmlTextWriterEndElement(writer);
		xmlTextWriterEndElement(writer);
		//end common

		//start trans
		xmlTextWriterStartElement(writer, BAD_CAST "trans");

		xmlTextWriterStartElement(writer, BAD_CAST "frmid");
		sprintf(tmpstr, "%d", j);
		xmlTextWriterWriteString(writer, BAD_CAST tmpstr);
		xmlTextWriterEndElement(writer);

		xmlTextWriterStartElement(writer, BAD_CAST "bc");
		sprintf(tmpstr, "%d", len);
		xmlTextWriterWriteString(writer, BAD_CAST tmpstr);
		xmlTextWriterEndElement(writer);

		xmlTextWriterStartElement(writer, BAD_CAST "ck");
		crc = crc16ModRtu((uint8*)file_buf, len);
		sprintf(tmpstr, "%02X %02X", (uint8)crc, crc>>8);
		xmlTextWriterWriteString(writer, BAD_CAST tmpstr);
		xmlTextWriterEndElement(writer);

		xmlTextWriterEndElement(writer);
		//end trans

		xmlTextWriterStartElement(writer, BAD_CAST "bin");
		xmlTextWriterWriteBase64(writer, file_buf, 0, len);
		xmlTextWriterEndElement(writer);
		xmlTextWriterEndDocument(writer);
		xmlFreeTextWriter(writer);
		sprintf(file, "%d", j);
		strcat(file, ".xml");
		xmlSaveFileEnc(file, doc, DEFAULT_ENCODING);
		xmlFreeDoc(doc);
		j++;
	}
	fclose(fp);
}

/*
获得当前时间字符串
@param buffer [out]: 时间字符串
@return 空
*/
void get_local_time(char* buffer)
{
	time_t rawtime;
	struct tm* timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
	(timeinfo->tm_year+1900), timeinfo->tm_mon+1, timeinfo->tm_mday,
	timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}
/*
获得文件大小
@param filename [in]: 文件名
@return 文件大小
*/
long get_file_size(char* filename)
{
	long length = 0;
	FILE *fp = NULL;
	fp = fopen(filename, "rb");
	if (fp != NULL) {
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
	}
	if (fp != NULL) {
		fclose(fp);
		fp = NULL;
	}
	return length;
}
/*
写入日志文件
@param filename [in]: 日志文件名
@param max_size [in]: 日志文件大小限制
@param buffer [in]: 日志内容
@param buf_size [in]: 日志内容大小
@return 空
*/
void write_log_file(char* buffer, unsigned buf_size)
{
	FILE *fp;
	char now[64];
	// 文件超过最大限制, 删除
	long length = get_file_size(LOG_FILE_NAME);
	if (length > FILE_MAX_SIZE) {
		unlink(LOG_FILE_NAME); // 删除文件
	}
	// 写日志
	fp = fopen(LOG_FILE_NAME, "at+");
	if (fp != NULL) {
		memset(now, 0, sizeof(now));
		get_local_time(now);
		strcat(now, "---->>>> ");
		fwrite(now, strlen(now)+1, 1, fp);
		fwrite(buffer, buf_size, 1, fp);
		fclose(fp);
		fp = NULL;
	}
}

void printBuf(uint8* buf, uint16 bufSize, const char* file, const char* func, uint32 line)
{
	uint16 i = 0;
	printf("[%s][%s][%d]buf: ", file, func, line);
	for (i = 0; i < bufSize; i++)
		printf("%02X ", buf[i]);
	printf("\n");
}

uint8 inverseArray(uint8* buf, uint16 bufSize)
{
	uint16 i = 0;

	if(NULL == buf)
		return ERR_1;

	for (i = 0;i < bufSize / 2; i++) {//swap two symmetric elements
		buf[i] = buf[i] ^ buf[bufSize - i - 1];
		buf[bufSize - i - 1] = buf[i] ^ buf[bufSize - i - 1];
		buf[i] = buf[i] ^ buf[bufSize - i - 1];
	}

	return NO_ERR;
}

uint8 inverseInt16(uint16 little, uint16* big)
{
	if(NULL == big)
		return ERR_1;

	if(ERR_1 == inverseArray((uint8*)&little, sizeof(uint16)))
		return ERR_1;

	*big = little;

	return NO_ERR;
}

uint8 inverseInt32(uint32 little, uint32* big)
{
	if(NULL == big)
		return ERR_1;

	if(ERR_1 == inverseArray((uint8*)&little, sizeof(uint32)))
		return ERR_1;

	*big = little;

	return NO_ERR;
}

uint8 inverseFloat32(float32 little, float32* big)
{
	if(NULL == big)
		return ERR_1;

	if(ERR_1 == inverseArray((uint8*)&little, sizeof(float32)))
		return ERR_1;

	*big = little;

	return NO_ERR;
}

