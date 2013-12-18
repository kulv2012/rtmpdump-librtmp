#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "librtmp/rtmp_sys.h"
#include "librtmp/log.h"

//网络与本地字节转换
#define HTON16(x)  ((x>>8&0xff)|(x<<8&0xff00))
#define HTON24(x)  ((x>>16&0xff)|(x<<16&0xff0000)|x&0xff00)
#define HTON32(x)  ((x>>24&0xff)|(x>>8&0xff00) | (x<<8&0xff0000)|(x<<24&0xff000000))

bool Read8(int *i8,FILE*fp);
bool Read16(int *i16,FILE*fp);
bool Read24(int *i24,FILE*fp);
bool Read32(int *i32,FILE*fp);
bool Peek8(int *i8,FILE*fp);
bool ReadTime(int *itime,FILE*fp);

//RTMP_XXX()返回0表示失败，1表示成功
RTMP*rtmp=NULL;//rtmp应用指针
RTMPPacket*packet=NULL;//rtmp包结构
char* rtmpurl="rtmp://127.0.0.1:1935/live/hwtest2";//连接的URL
//char* flvfile="xiaokai.flv";//读取的flv文件
//char* flvfile="130038728.mp3";//读取的flv文件

bool ZINIT();//初始化相关
void ZCLEAR();//清除相关

int main(int argc, char* argv[])
{
	if (!ZINIT())
	{
		printf("init socket err\n");
		return -1;
	}
/////////////////////////////////初始化//////////////////////	
	RTMP_LogLevel lvl=RTMP_LOGINFO;
	RTMP_LogSetLevel(lvl);//设置信息等级
	//RTMP_LogSetOutput(FILE*fp);//设置信息输出文件
	rtmp=RTMP_Alloc();//申请rtmp空间
	RTMP_Init(rtmp);//初始化rtmp设置
	rtmp->Link.timeout=5;//设置连接超时，单位秒，默认30秒
	packet= malloc(sizeof(RTMPPacket) ) ;//new RTMPPacket();//创建包
	RTMPPacket_Alloc(packet,1024*64);//给packet分配数据空间，要满足最长的帧，不知道可设大些
	RTMPPacket_Reset(packet);//重置packet状态
////////////////////////////////连接//////////////////
	RTMP_SetupURL(rtmp,rtmpurl);//设置url
	RTMP_EnableWrite(rtmp);//设置可写状态
	if (!RTMP_Connect(rtmp,NULL))//连接服务器
	{
		printf("connect err\n");
		ZCLEAR();
		return -1;
	}
	if (!RTMP_ConnectStream(rtmp,0))//创建并发布流(取决于rtmp->Link.lFlags)
	{
		printf("ConnectStreamerr\n");
		ZCLEAR();
		return -1;
	}
	packet->m_hasAbsTimestamp = 0; //绝对时间戳
	packet->m_nChannel = 0x04; //通道
	packet->m_nInfoField2 = 0;

	FILE*fp=fopen(argv[1],"rb");
	if (fp==NULL)
	{
		printf("open file:%s err\n",argv[1]);
		ZCLEAR();
		return -1;
	}
	fseek(fp,0,SEEK_SET);

	printf("rtmpurl:%s\nflvfile:%s\nsend data ...\n",rtmpurl, argv[1]);
////////////////////////////////////////发送数据//////////////////////
	while(TRUE)
	{
	/*	int type=0;//类型
		int datalength=0;//数据长度
		int time=0;//时间戳
		int streamid=0;//流ID
		if(!Read8(&type,fp))
			break;
		if(!Read24(&datalength,fp))
			break;
		if(!ReadTime(&time,fp))
			break;
		if(!Read24(&streamid,fp))
			break;

		if (type!=0x08&&type!=0x09)
		{
			fseek(fp,datalength+4,SEEK_CUR);
			continue;
		}
*/
		int datalength = fread(packet->m_body,1,1024,fp) ;
		if( datalength <= 0 ) 
			break ;
		packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM; 
		packet->m_nTimeStamp = 0; 
		packet->m_packetType= 0x08;
		packet->m_nBodySize = datalength;

		if (!RTMP_IsConnected(rtmp))
		{
			printf("rtmp is not connect\n");
			break;
		}
		if (!RTMP_SendPacket(rtmp,packet,0))
		{
			printf("send err\n");
			break;
		}
		printf("RTMP_SendPacket success.length=%d\n", packet->m_nBodySize );
	}
	printf("send data over\n");
	fclose(fp);
	ZCLEAR();
	return 0;
}

bool ZINIT()
{
	return TRUE;
}
void ZCLEAR()
{
	//////////////////////////////////////////释放/////////////////////
	if (rtmp!=NULL)
	{
		RTMP_Close(rtmp);//断开连接
		RTMP_Free(rtmp);//释放内存
		rtmp=NULL;
	}
	if (packet!=NULL)
	{
		RTMPPacket_Free(packet);//释放内存
		free( packet );
		packet=NULL;
	}
	///////////////////////////////////////////////////
}


bool Read8(int *i8,FILE*fp)
{
	if(fread(i8,1,1,fp)!=1)
		return false;
	return true;
}
bool Read16(int *i16,FILE*fp)
{
	if(fread(i16,2,1,fp)!=1)
		return false;
	*i16=HTON16( (*i16) );
	return true;
}
bool Read24(int *i24,FILE*fp)
{
	if(fread(i24,3,1,fp)!=1)
		return false;
	*i24 = HTON24( (*i24) );
	return true;
}
bool Read32(int *i32,FILE*fp)
{
	if(fread(i32,4,1,fp)!=1)
		return false;
	*i32=HTON32( (*i32) );
	return true;
}
bool Peek8(int *i8,FILE*fp)
{
	if(fread(i8,1,1,fp)!=1)
		return false;
	fseek(fp,-1,SEEK_CUR);
	return true;
}
bool ReadTime(int *itime,FILE*fp)
{
	int temp=0;
	if(fread(&temp,4,1,fp)!=1)
		return false;
	*itime=HTON24(temp);
	*itime|=(temp&0xff000000);
	return true;
}
