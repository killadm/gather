/*
  *******************************************************************************
  * @file    xml.c
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

#include "includes.h"
#include "queue.h"
#include "commap.h"


char *pXMLFileName[XML_BUF_FILE_NUM]={"buff0.xml","buff1.xml","buff2.xml","buff3.xml","buff4.xml","buff5.xml",
 		"buff6.xml","buff7.xml","buff8.xml","buff9.xml","buff10.xml","buff11.xml","buff12.xml",
 		"buff13.xml","buff14.xml"};

XML_FILE gXML_File[XML_BUF_FILE_NUM];  //定义xml文件相关变量。
static xml_info_str g_xml_info[UP_COMMU_DEV_ARRAY];

uint8 func_id(uint8 dev, uint8 xml_idx);
uint8 func_heart_beat(uint8 dev, uint8 xml_idx);
uint8 func_sysconfig(uint8 dev, uint8 xml_idx);
uint8 func_rqdata(uint8 dev, uint8 xml_idx);
uint8 func_tnode(uint8 dev, uint8 xml_idx);
uint8 func_minfo(uint8 dev, uint8 xml_idx);
uint8 func_rptup(uint8 dev, uint8 xml_idx);
uint8 func_rdstat(uint8 dev, uint8 xml_idx);
uint8 func_swip(uint8 dev, uint8 xml_idx);
uint8 func_dbmani(uint8 dev, uint8 xml_idx);
uint8 func_syscmd(uint8 dev, uint8 xml_idx);
uint8 func_codeup(uint8 dev, uint8 xml_idx);
uint8 func_prototrs(uint8 dev, uint8 xml_idx);

static uint8 (*xml_exec_array[])(uint8 dev, uint8 xml_idx) = {
func_id,//登录
func_heart_beat,//心跳
func_sysconfig,//系统参数配置
func_rqdata,//仪表的数据项配置
func_tnode,//抄表与上报时间点配置
func_minfo,//表地址配置
func_rptup,//上传历史数据
func_rdstat,//读取集中器状态
func_swip,//切换ip
func_dbmani,//数据库透传
func_syscmd,//本地shell命令透传
func_codeup,//程序更新
func_prototrs//协议透传
};


/*
  ******************************************************************************
  * 函数名称： uint8 XMLBuf_Init(void)
  * 说    明： 初始化XML  BUF，只允许在系统初始化时初始化一次，运行中不允许调用。
  * 参    数： 无
  ******************************************************************************
*/
static uint8 gsu8XmlBufInitFlag = 0;//用于标记XMLBuf_Init是否已经被调用，因为该函数只允许调用一次。
uint8 XMLBuf_Init(void)
{
	uint8 i = 0;
	int ret = 0; 

	if(gsu8XmlBufInitFlag != 0)  //如果已经被调用，再次调用则返回错误。
		return ERR_FF;

	for(i=0;i<XML_BUF_FILE_NUM;i++){
		gXML_File[i].usedflag = 0;

		ret = sem_init(&(gXML_File[i].sem_read),0,1);
		if(ret != 0){
	   		perror("sem_init\n");
		}

		ret = sem_init(&(gXML_File[i].sem_write),0,1);
		if(ret != 0){
	   		perror("sem_init\n");
		}

		gXML_File[i].pXMLFile = pXMLFileName[i];

		//printf("i=%d.\n",i);  //调试跟踪用。

	}

	gsu8XmlBufInitFlag = 1;

	return NO_ERR;

}

/*
  ******************************************************************************
  * 函数名称： uint8 Get_XMLBuf(void)
  * 说    明： 用于获取一个可用的XML Buf序号。
  * 参    数： 返回可用的XML Buf序号。
  			如果返回0xff，说明当前没有可用的buf.
  ******************************************************************************
*/
uint8 Get_XMLBuf(void)
{
	uint8 i = 0;
	uint8 ret = ERR_FF;

	OS_ENTER_CRITICAL();
	for(i=0;i<XML_BUF_FILE_NUM;i++){
		if(gXML_File[i].usedflag == 0){
			gXML_File[i].usedflag = 1;  //标记为占用。
			ret = i;	
			break;
		}
	}
	OS_EXIT_CRITICAL();

	return ret;
}


/*
  ******************************************************************************
  * 函数名称： uint8 Put_XMLBuf(uint8 lu8BufIndex)
  * 说    明： 用于释放一个已被占用的XML Buf序号。
  * 参    数： 返回释放释放成功。
  			如果返回0xff，说明输入参数超限。
  ******************************************************************************
*/
uint8 Put_XMLBuf(uint8 lu8BufIndex)
{

	if(lu8BufIndex >= XML_BUF_FILE_NUM) //超限检查，防止错误。
		return ERR_FF;

	OS_ENTER_CRITICAL();
	gXML_File[lu8BufIndex].usedflag = 0;
	OS_EXIT_CRITICAL();

	return NO_ERR;

}
		
		
		
		
uint8 UpGetXMLStart(uint8 XmlIndex,uint8 dev, uint32 OutTime)
{
	uint8 err = 0x00;
	int8  XMLHead[] ="<?xml version";//=\"1.0\" encoding=\"utf-8\" ?>";
	uint8 DataTemp = 0x00;
	uint8 Flag = 0x01;
	uint32 i;
	uint32 lu32tmp = 0;
	FILE *fp;

	lu32tmp = strlen(XMLHead);
	while(Flag){		
		for(i=0;i<lu32tmp;i++){
			while(QueueRead(&DataTemp, (void*)pQueues[dev]) != QUEUE_OK){
				OSSemPend(dev, OutTime, &err);
				if(err != OS_ERR_NONE){
					//printf("[%s][%s][%d]OSSemPend err=%d\n",FILE_LINE,err);
					return err;
				}
			}
			   
			if(DataTemp == XMLHead[i])
			{;}
			else{
				i=0;
				continue;
			}
		}
		
		Flag = 0;
	}
	
	printf("start fopen %s . \n", gXML_File[XmlIndex].pXMLFile);
	fp=fopen(gXML_File[XmlIndex].pXMLFile,"w+");
	
	sem_wait(&gXML_File[XmlIndex].sem_write);
	fwrite(XMLHead,1,strlen(XMLHead),fp);
	sem_post(&gXML_File[XmlIndex].sem_write);
	fclose(fp);
	
	return NO_ERR;
	
}



		
		
uint8 UpGetXMLEnd(uint8 XmlIndex,uint8 dev, uint32 OutTime)
{
	uint8 err = 0x00;
	int8  XMLEnd[] ="</root>";	// </test>
	uint8 DataTemp = 0x00;
	uint8 Flag = 0x01;
	uint32 i;
	int j = 0;
	uint32 lu32tmp = 0;
	FILE *fp;

	lu32tmp = strlen(XMLEnd);
	while(Flag){		
		for(i=0;i<lu32tmp;i++){
			while(QueueRead(&DataTemp, (void*)pQueues[dev]) != QUEUE_OK){
				OSSemPend(dev, OutTime, &err);
				if(err != OS_ERR_NONE){
					printf("[%s][%s][%d]OSSemPend err=%d\n",FILE_LINE,err);
					return err;
				}
			}
			
			*(pRecvTempBuf[dev] + j++) = DataTemp;
			
			if(j == (RECV_TEMPBUF_SIZE - 1)){
				fp = fopen(gXML_File[XmlIndex].pXMLFile,"a+");
				debug_info(gDebugModule[XML_MODULE], "xml name is %s\n", gXML_File[XmlIndex].pXMLFile);
				sem_wait(&gXML_File[XmlIndex].sem_write);
				fwrite(pRecvTempBuf[dev],1,RECV_TEMPBUF_SIZE,fp);
				sem_post(&gXML_File[XmlIndex].sem_write);
				debug_err(gDebugModule[XML_MODULE], "[%s][%s][%d] fwrite Lens=%d\n",FILE_LINE,RECV_TEMPBUF_SIZE);
				memset(pRecvTempBuf[dev],0,RECV_TEMPBUF_SIZE);
				j=0;
				fclose(fp);
			}
			
			if(DataTemp == XMLEnd[i])
			{;}
			else{
				i=0;
				continue;
			}
		   
		}
		
		Flag = 0;
	}
	
	if(j != 0){
		fp=fopen(gXML_File[XmlIndex].pXMLFile,"a+");
		debug_info(gDebugModule[XML_MODULE], "xml name is %s\n", gXML_File[XmlIndex].pXMLFile);
		sem_wait(&gXML_File[XmlIndex].sem_write);
		fwrite(pRecvTempBuf[dev],1,j,fp);
		sem_post(&gXML_File[XmlIndex].sem_write);
		debug_err(gDebugModule[XML_MODULE], "[%s][%s][%d] fwrite Lens=%d\n",FILE_LINE,RECV_TEMPBUF_SIZE);
		memset(pRecvTempBuf[dev],0,RECV_TEMPBUF_SIZE);
		j=0;
		fclose(fp);
	}

	
	return NO_ERR;

	
}




/*
  ******************************************************************************
  * 函数名称： makexml
  * 说    明： 生成xml文件
  * 参    数： 
  *      		optype--操作类型
  ******************************************************************************
*/
uint8 makexml(xml_info_str *xmlInfo,uint8 xmlIndex)
{
	sys_config_str sysConfig;
	FILE *fp;
	int nRel;
	//定义文档和指针
	xmlDocPtr doc = xmlNewDoc(BAD_CAST"1.0");
	//新建节点
    	xmlNodePtr root_node = xmlNewNode(NULL,BAD_CAST"root");
    	//设置新节点为根节点
    	xmlDocSetRootElement(doc,root_node);
    	//创建一个新节点common,添加到根节点
    	xmlNodePtr node = xmlNewNode(NULL,BAD_CAST "common");
	xmlAddChild(root_node,node);

	fp = fopen(gXML_File[xmlIndex].pXMLFile,"w+");
	
	
	switch(xmlInfo->func_type){
		case em_FUNC_ID:
			//在common节点直接创建文本节点
			get_sys_config(CONFIG_GATEWAY_ID,&sysConfig);
    			xmlNewTextChild(node,NULL,BAD_CAST "sadd",(xmlChar *)sysConfig.f_config_value);
    			xmlNewTextChild(node,NULL,BAD_CAST "oadd",(xmlChar *)sysConfig.f_config_value);  //目的地址以后改成服务器地址。
			xmlNewTextChild(node,NULL,BAD_CAST "func_type",(xmlChar *)"0");
			xmlNewTextChild(node,NULL,BAD_CAST "oper_type",(xmlChar *)"0");

    			//debug_info(gDebugModule[XML_MODULE], "[%s][%s][%d] optype is %d\n",FILE_LINE, optype);

			nRel = xmlSaveFileEnc(gXML_File[xmlIndex].pXMLFile,doc,"utf-8");
			fclose(fp);
			if(nRel != -1){
				xmlFreeDoc(doc);
				printf("make xml success.xml Index = %d.\n",xmlIndex);
				return NO_ERR;
			} 
			
			break;

		case em_FUNC_HEATBT:
			//在common节点直接创建文本节点
			get_sys_config(CONFIG_GATEWAY_ID,&sysConfig);
            xmlNewTextChild(node,NULL,BAD_CAST "sadd",(xmlChar *)sysConfig.f_config_value);
            xmlNewTextChild(node,NULL,BAD_CAST "oadd",(xmlChar *)sysConfig.f_config_value);//目的地址以后改成服务器地址。
			xmlNewTextChild(node,NULL,BAD_CAST "func_type",(xmlChar *)"1");
			xmlNewTextChild(node,NULL,BAD_CAST "oper_type",(xmlChar *)"0");

            //debug_info(gDebugModule[XML_MODULE], "[%s][%s][%d] optype is %d\n",FILE_LINE, optype);

			nRel = xmlSaveFileEnc(gXML_File[xmlIndex].pXMLFile,doc,"utf-8");
			fclose(fp);
			if(nRel != -1){
				xmlFreeDoc(doc);
				printf("make xml success.xml Index = %d.\n",xmlIndex);
				return NO_ERR;
			} 

			break;
        case em_FUNC_RPTUP:
            
		default:
			fclose(fp);
			break;
			


	}



  	return ERR_1;
}

/*
  ******************************************************************************
  * 函数名称： uint8 setXmlInfo()
  * 说    明： 根据设备号，设置相应的gXmlInfo信息。
  * 参    数： 
  ******************************************************************************
*/

uint8 setXmlInfo(uint8 dev, pXml_info pXml_info)
{
	if(dev >= UP_COMMU_DEV_ARRAY){
		printf("dev num error.\n");
		return ERR_1;
	}
	if(NULL == pXml_info) {
		printf("source info is NULL.\n");
		return ERR_1;
	}
	memcpy((uint8 *)&g_xml_info[dev],(uint8 *)pXml_info,sizeof(xml_info_str));
	return NO_ERR;
}

/*
  ******************************************************************************
  * 函数名称： uint8 getXmlInfo(uint8 dev,xml_info_str *xmlInfo)
  * 说    明： 根据设备编号，获取对应的xmlInfo信息。
  * 参    数： 
  ******************************************************************************
*/

uint8 getXmlInfo(uint8 dev,xml_info_str *pXml_info)
{

	if(dev >= UP_COMMU_DEV_ARRAY){
		printf("dev num error.\n");
		return ERR_1;
	}

	memcpy((uint8 *)pXml_info,(uint8 *)&g_xml_info[dev],sizeof(xml_info_str));

	return NO_ERR;

}



/*
 ******************************************************************************
 * 函数名称： parse_xml
 * 说	  明： 解析并执行xml文件
 ******************************************************************************
 */

uint8 func_id(uint8 dev, uint8 xml_idx)
{
	uint8 err = NO_ERR;
	FILE *fp;
	xml_info_str l_xmlInfo;
	err = getXmlInfo(dev,&l_xmlInfo);
	if(err != NO_ERR){
		printf("XmlInfo_Exec getXmlInfo Err.\n");
		return err;
	}
	printf("now in func_id().\n");
	printf("g_xml_info[%d].func_type: %u, g_xml_info[%d].oper_type: %u\n", dev, g_xml_info[dev].func_type, dev, g_xml_info[dev].oper_type);
	switch(g_xml_info[dev].oper_type){
	case em_OPER_RD:
		err = makexml(&l_xmlInfo,xml_idx);
		if(err == NO_ERR){
			fp = fopen(gXML_File[xml_idx].pXMLFile,"r");
			FileSend(dev, fp);
			fclose(fp);
		}		
	case em_OPER_WR:
		break;
	case em_OPER_ASW:
		printf("have read func_id answer from server.\n");//接收到应答, 登录成功
		sem_post(&Validate_sem);
		break;
	default:
		err = ERR_FF;
	}
	return err;
}

uint8 func_heart_beat(uint8 dev, uint8 xml_idx)
{
	uint8 err = NO_ERR;
	FILE *fp;
	xml_info_str l_xmlInfo;
	
	err = getXmlInfo(dev,&l_xmlInfo);
	if(err != NO_ERR){
		printf("XmlInfo_Exec getXmlInfo Err.\n");
		return err;
	}
	printf("now in func_heart_beat().\n");
	printf("l_xmlInfo.func_type: %u, l_xmlInfo.oper_type: %u\n", l_xmlInfo.func_type, l_xmlInfo.oper_type);
	switch(l_xmlInfo.oper_type){
	case em_OPER_RD:
		printf("now make heart_beat xml.\n");
		err = makexml(&l_xmlInfo,xml_idx);
		if(err == NO_ERR){
			fp = fopen(gXML_File[xml_idx].pXMLFile,"r");
			FileSend(dev, fp);
			fclose(fp);
		}		
	case em_OPER_WR:
		break;
	case em_OPER_ASW:
		printf("have read heart_beat answer from server.\n");//接收到应答, 本次心跳成功
		//清空看门狗
		UpdGprsRunSta_FeedSndDog();
		break;
	default:
		err = ERR_FF;
	}
	return err;
}

//3．配置信息(sys_config)
uint8 func_sysconfig(uint8 dev, uint8 xml_idx)
{
		uint8 retErr = NO_ERR;
		
		return retErr;
}
//4. 配置需要哪些物理量(request_data)
uint8 func_rqdata(uint8 dev, uint8 xml_idx)
{
	uint8 err = NO_ERR;
	
	return err;
}
//5. 配置抄表时间点(time_node)
uint8 func_tnode(uint8 dev, uint8 xml_idx)
{
		uint8 retErr = NO_ERR;
		
		return retErr;
}
//6. 操作表地址信息(meter_info)
uint8 func_minfo(uint8 dev, uint8 xml_idx)
{
		uint8 retErr = NO_ERR;
		
		return retErr;
}

uint8 get_tm_node(uint8 dev, char* timenode) {
    xmlDocPtr pDoc = g_xml_info[dev].xmldoc_rd;
    xmlNodePtr curNode = xmlDocGetRootElement(pDoc);
    curNode = curNode->children;//<common>node
    while(curNode) {
        if(xmlStrEqual(curNode->name, CONST_CAST "time")) {
            strcpy(timenode, (char*)xmlNodeGetContent(curNode->xmlChildrenNode));
        }
        curNode = curNode->next;
    }
    return NO_ERR;
}

//向xmldoc中添加一行的数据节点
int wr_his_xml(pHis_data pHis, uint8 dev)
{
    printf("now in wr_his_xml()\n");
    FILE *fp;
    int nRel;
    sys_config_str sysConfig;
    char str[100];//写入func_type或者oper_type
    xmlNodePtr root_node;
    xmlNodePtr common_node;
    xmlNodePtr trans_node;
    xmlNodePtr row_node;
    int val_idx=0;//历史数据项的索引
    printf("current state: %d\n", g_xml_info[dev].cur_wr_state);
    switch (g_xml_info[dev].cur_wr_state) {
    case stat_his_init:
        g_xml_info[dev].xmldoc_wr = xmlNewDoc(BAD_CAST"1.0");
        root_node = xmlNewNode(NULL,BAD_CAST"root");
        xmlDocSetRootElement(g_xml_info[dev].xmldoc_wr,root_node);
        common_node = xmlNewNode(NULL,BAD_CAST "common");
        xmlAddChild(root_node,common_node);
        get_sys_config(CONFIG_GATEWAY_ID,&sysConfig);
        xmlNewTextChild(common_node,NULL,BAD_CAST "sadd",(xmlChar *)sysConfig.f_config_value);
        get_sys_config(CONFIG_PRIMARY_SERVER,&sysConfig);
        xmlNewTextChild(common_node,NULL,BAD_CAST "oadd",(xmlChar *)sysConfig.f_config_value);  //目的地址以后改成服务器地址。
        sprintf(str, "%d", em_FUNC_RPTUP);
        xmlNewTextChild(common_node,NULL,BAD_CAST "func_type",(xmlChar *)str);
        get_sys_config(CONFIG_REPORT_MODE,&sysConfig);//数据上报模式,0-主动上报, 1被动请求
        sprintf(str, "%d", (atoi(sysConfig.f_config_value) ? em_OPER_ASW : em_OPER_WR));//主动上报, 就是写入; 被动请求, 应答
        xmlNewTextChild(common_node,NULL,BAD_CAST "oper_type",(xmlChar *)str);
        g_xml_info[dev].cur_wr_state = stat_his_trans;
        break;
    case stat_his_trans:
        root_node = xmlDocGetRootElement(g_xml_info[dev].xmldoc_wr);
        trans_node = xmlNewNode(NULL,BAD_CAST "trans");
        xmlAddChild(root_node,trans_node);
        sprintf(str, "%d", g_xml_info[dev].total_rows);
        xmlNewTextChild(trans_node, NULL, BAD_CAST "total_meter_num", (xmlChar *)str);//总共要上报的抄表数据条数
        sprintf(str, "%d", g_xml_info[dev].cur_frame);
        xmlNewTextChild(trans_node, NULL, BAD_CAST "frame_idx", (xmlChar *)str);//本帧的索引
        sprintf(str, "%d", g_xml_info[dev].cur_cnt);
        xmlNewTextChild(trans_node, NULL, BAD_CAST "meter_num", (xmlChar *)str);//本帧共包含多少条抄表数据
        xmlNewTextChild(trans_node, NULL, BAD_CAST "time_node", (xmlChar *)(g_xml_info[dev].timenode));//上报的是哪个时间点的数据
        g_xml_info[dev].cur_wr_state = stat_his_data;
        break;
    case stat_his_data:
        g_xml_info[dev].cur_row_idx++;
        root_node = xmlDocGetRootElement(g_xml_info[dev].xmldoc_wr);
        row_node = xmlNewNode(NULL,BAD_CAST "row");
        sprintf(str, "%d", g_xml_info[dev].cur_row_idx);
        xmlNewProp(row_node, BAD_CAST "id", BAD_CAST str);//当前帧的第几行
        xmlAddChild(root_node,row_node);
        //固定项
        xmlNewTextChild(row_node, NULL, CONST_CAST "f_id", CONST_CAST pHis->f_id);//row_id
        xmlNewTextChild(row_node, NULL, CONST_CAST "f_meter_type", CONST_CAST pHis->f_meter_type);//表类型
        xmlNewTextChild(row_node, NULL, CONST_CAST "f_device_id", CONST_CAST pHis->f_device_id);//设备编号
        xmlNewTextChild(row_node, NULL, CONST_CAST "f_meter_address", CONST_CAST pHis->f_meter_address);//表地址
        xmlNewTextChild(row_node, NULL, CONST_CAST "f_timestamp", CONST_CAST pHis->f_timestamp);//时间戳
        xmlNewTextChild(row_node, NULL, CONST_CAST "f_time", CONST_CAST pHis->f_time);//抄表时间点
        for(val_idx=0;val_idx<pHis->value_cnt;val_idx++) {
            xmlNewTextChild(row_node, NULL, CONST_CAST pHis->value_list[val_idx].field_name, \
            CONST_CAST pHis->value_list[val_idx].field_value);//数据项
        }
        
        if(g_xml_info[dev].cur_row_idx == g_xml_info[dev].cur_cnt)
            g_xml_info[dev].cur_wr_state = stat_his_end;
        break;
    case stat_his_end:
        printf("now in stat_his_end, and write to xmlfile: %s\n", gXML_File[g_xml_info[dev].xml_wr_file_idx].pXMLFile);
        fp = fopen(gXML_File[g_xml_info[dev].xml_wr_file_idx].pXMLFile,"w+");
        nRel = xmlSaveFileEnc(gXML_File[g_xml_info[dev].xml_wr_file_idx].pXMLFile, g_xml_info[dev].xmldoc_wr,"utf-8");
        fclose(fp);
        if(nRel != -1){
            xmlFreeDoc(g_xml_info[dev].xmldoc_wr);
            printf("make xml success.xml Index = %d.\n", g_xml_info[dev].xml_wr_file_idx);
            return NO_ERR;
        }
        break;
    default:
        break;
    }
    return nRel;
}

//抄表数据上传
uint8 func_rptup(uint8 dev, uint8 xml_idx)
{
	uint8 err = NO_ERR;
    FILE *fp;
    char pErr[100];
    uint8 lu8xmlIndex; 
	if(err != NO_ERR){
		printf("getXmlInfo Error.\n");
		return err;
	}
	printf("now in func_rptup().\n");
	printf("g_xml_info[dev].oper_type: %d\n", g_xml_info[dev].oper_type);
	switch(g_xml_info[dev].oper_type){
	case em_OPER_RD:
        err = get_tm_node(dev, g_xml_info[dev].timenode);
        printf("g_xml_info[dev].timenode: %s\n", g_xml_info[dev].timenode);
        /* 1. 计算所有仪表类型总共需要上传多少行数据, 
         * 总共有多少帧, 当前帧是多少, 当前行数是多少
         * 2. 按照仪表的类型依次读取历史数据, 
         */
        read_his_data(g_xml_info[dev].timenode, em_heat, pErr);
        printf("read_his_data OK \n");
        g_xml_info[dev].total_rows = get_his_cnt(em_heat);
        printf("g_xml_info[dev].total_rows: %d\n", g_xml_info[dev].total_rows);
        g_xml_info[dev].mod = g_xml_info[dev].total_rows%ROW_PER_FRAME;
        printf("g_xml_info[dev].mod: %d\n", g_xml_info[dev].mod);
        g_xml_info[dev].total_frame = (g_xml_info[dev].total_rows/ROW_PER_FRAME+(g_xml_info[dev].mod?1:0));
        printf("g_xml_info[dev].total_frame: %d\n", g_xml_info[dev].total_frame);
        g_xml_info[dev].cur_frame = 0;
        
        while(g_xml_info[dev].cur_frame<g_xml_info[dev].total_frame){
            g_xml_info[dev].cur_wr_state = stat_his_init;//初始状态
            printf("g_xml_info[dev].cur_wr_state: %d\n", g_xml_info[dev].cur_wr_state);
            g_xml_info[dev].cur_row_idx = 0;
            printf("g_xml_info[dev].cur_row_idx: %d\n", g_xml_info[dev].cur_row_idx);
            wr_his_xml(NULL, dev);//写root节点和common节点
            g_xml_info[dev].cur_frame++;
            g_xml_info[dev].cur_cnt = (g_xml_info[dev].cur_frame<=(g_xml_info[dev].total_frame-1) ? ROW_PER_FRAME:g_xml_info[dev].mod);
            wr_his_xml(NULL, dev);//写trans节点
            g_xml_info[dev].cur_row_idx = 0;
            retrieve_and_del_his_data(em_heat, g_xml_info[dev].cur_cnt, wr_his_xml, dev);//写row节点
            printf("retrieve_his_data over.\n");

            do{
                lu8xmlIndex = Get_XMLBuf();  //获取一个xml暂存空间,最后一定要释放该空间，获取-使用-释放。
            }while(lu8xmlIndex == ERR_FF);
            g_xml_info[dev].xml_wr_file_idx = lu8xmlIndex; 
            printf("call wr_his_xml to write xml to file.\n");
            wr_his_xml(NULL, dev);//将xmldoc写到文件
            if(err == NO_ERR) {//发送文件
                fp = fopen(gXML_File[xml_idx].pXMLFile,"r");
                FileSend(dev, fp);
                fclose(fp);
            }
            //wait answer from server for 10 sec, if (OK) continue; else send again
            Put_XMLBuf(lu8xmlIndex);  //释放被占用的xml暂存.
            g_xml_info[dev].cur_rows += g_xml_info[dev].cur_cnt;//当前已发送的行数
            printf("make xml over!!!!!!!!\n");
		}
        break;
	case em_OPER_WR:
        break;
	case em_OPER_ASW:
        printf("have read history report answer from server.\n");
        UpdGprsRunSta_FeedSndDog();
        break;
	default:
		err = ERR_FF;
	}
	return err;
}

uint8 func_rdstat(uint8 dev, uint8 xml_idx)
{
		uint8 retErr = NO_ERR;
		
		return retErr;
}

uint8 func_swip(uint8 dev, uint8 xml_idx)
{
		uint8 retErr = NO_ERR;
		
		return retErr;
}

uint8 func_dbmani(uint8 dev, uint8 xml_idx)
{
		uint8 retErr = NO_ERR;
		
		return retErr;
}

uint8 func_syscmd(uint8 dev, uint8 xml_idx)
{
		uint8 retErr = NO_ERR;
		
		return retErr;
}

uint8 func_codeup(uint8 dev, uint8 xml_idx)
{
		uint8 retErr = NO_ERR;
		
		return retErr;
}

uint8 func_prototrs(uint8 dev, uint8 xml_idx)
{
		uint8 retErr = NO_ERR;
		
		return retErr;
}



uint8 xml_exec(uint8 dev, uint8 xml_idx)
{
	if(dev >= UP_COMMU_DEV_ARRAY){
		printf("dev num error.\n");
		return ERR_1;
	}
	
	printf("g_xml_info[dev].func_type: %u\n", g_xml_info[dev].func_type);
	
	return (*xml_exec_array[g_xml_info[dev].func_type])(dev, xml_idx);
}

uint8 parse_common(uint8 dev, uint8 xml_idx, xmlNodePtr common_node)
{
	uint8 retErr = NO_ERR;
	xmlNodePtr cur_node;
	xmlChar* pValue;
	sys_config_str sysConfig;
		
	cur_node = common_node->xmlChildrenNode;
	while(cur_node != NULL){
		if(xmlStrEqual(cur_node->name, CONST_CAST NODE_SRC_ADDR)){
			pValue = xmlNodeGetContent(cur_node->xmlChildrenNode);
			strcpy((char*)g_xml_info[dev].sadd, (char*)pValue);
			xmlFree(pValue);
		}
		else if(xmlStrEqual(cur_node->name, CONST_CAST NODE_OBJ_ADDR)){
			pValue = xmlNodeGetContent(cur_node->xmlChildrenNode);
			strcpy((char*)g_xml_info[dev].oadd, (char*)pValue);
			xmlFree(pValue);
		}
		else if(xmlStrEqual(cur_node->name, CONST_CAST NODE_FUNC_TYPE)){
			pValue = xmlNodeGetContent(cur_node->xmlChildrenNode);
			g_xml_info[dev].func_type = atoi((char*)pValue);
			xmlFree(pValue);
		}
		else if(xmlStrEqual(cur_node->name, CONST_CAST NODE_OPER_TYPE)){
			pValue = xmlNodeGetContent(cur_node->xmlChildrenNode);
			g_xml_info[dev].oper_type = atoi((char*)pValue);
			xmlFree(pValue);
		}
		else{//异常情况
			retErr = NO_ERR;
		}
		cur_node=cur_node->next;
	}
	//正常应该先检查是不是发送给本集中器的数据，不是则舍弃，是的话才继续。
	//if (oadd is not me){return ERR_NOT_ME;}
	retErr = get_sys_config(CONFIG_GATEWAY_ID,&sysConfig);
    
	if(retErr == NO_ERR){
		if(0 == strcmp((char *)sysConfig.f_config_value, (char *)g_xml_info[dev].oadd)){  //只有是发给本集中器的数据才处理。
			printf("start  xml_exec().\n");			
			retErr = xml_exec(dev, xml_idx);
			printf((retErr==NO_ERR)?"parse xml success\n":"parse xml fail\n");

		}
		else{
			//不是给本集中器的，则空处理即可，舍弃。
		}
	}	

	return retErr;
}


uint8 parse_xml(uint8 dev, uint8 xml_idx)
{
    
	if(dev >= UP_COMMU_DEV_ARRAY){
		printf("dev num error.\n");
		return ERR_1;
	}
	
	if(dev == UP_COMMU_DEV_GPRS)
			UpdGprsRunSta_FeedSndDog();  //有任何GPRS网络数据收到，都认为网络正常，清空GPRS心跳计数。如果连续几次心跳都不清空，则GPRS重启。
	

	//sem_wait(fd's xml_info_str read semaphore)
	printf("current file name : %s\n", gXML_File[xml_idx].pXMLFile);
	xmlDocPtr doc;
	xmlNodePtr cur;
	uint8 retErr;
	doc = xmlParseFile(gXML_File[xml_idx].pXMLFile);
	if(doc == NULL) {
		fprintf(stderr, "Document not open successfully. \n");
		//sem_post(fd's xml_info_str read semaphore)
		//Put_XMLBuf
		return ERR_FF;
	}
	g_xml_info[dev].xmldoc_rd = doc;

	cur = xmlDocGetRootElement(doc);
	if(cur == NULL) {
		fprintf(stderr, "empty document\n");
		xmlFreeDoc(doc);
		//sem_post(fd's xml_info_str read semaphore)
		//Put_XMLBuf
		return ERR_FF;
	}

	if(!xmlStrEqual(cur->name, CONST_CAST NODE_ROOT)) {
		fprintf(stderr, "root node not found");
		xmlFreeDoc(doc);
		//sem_post(fd's xml_info_str read semaphore)
		//Put_XMLBuf
		return ERR_1;
	}
	cur = cur->xmlChildrenNode;
	while(cur != NULL){
		if(xmlStrEqual(cur->name, CONST_CAST NODE_COMMON)) {
			retErr = parse_common(dev, xml_idx, cur);
			break;
		}
		cur = cur->next;
	}
	//sem_post(fd's xml_info_str read semaphore)
	xmlFreeDoc(doc);
	
	return retErr;
}


