#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "librtmp/rtmp_sys.h"
#include "librtmp/log.h"

//�����뱾���ֽ�ת��
#define HTON16(x)  ((x>>8&0xff)|(x<<8&0xff00))
#define HTON24(x)  ((x>>16&0xff)|(x<<16&0xff0000)|x&0xff00)
#define HTON32(x)  ((x>>24&0xff)|(x>>8&0xff00) | (x<<8&0xff0000)|(x<<24&0xff000000))

bool Read8(int *i8,FILE*fp);
bool Read16(int *i16,FILE*fp);
bool Read24(int *i24,FILE*fp);
bool Read32(int *i32,FILE*fp);
bool Peek8(int *i8,FILE*fp);
bool ReadTime(int *itime,FILE*fp);

//RTMP_XXX()����0��ʾʧ�ܣ�1��ʾ�ɹ�
RTMP*rtmp=NULL;//rtmpӦ��ָ��
RTMPPacket*packet=NULL;//rtmp���ṹ
char* rtmpurl="rtmp://127.0.0.1:1935/live/hwtest2";//���ӵ�URL
//char* flvfile="xiaokai.flv";//��ȡ��flv�ļ�
//char* flvfile="130038728.mp3";//��ȡ��flv�ļ�

bool ZINIT();//��ʼ�����
void ZCLEAR();//������

int main(int argc, char* argv[])
{
	if (!ZINIT())
	{
		printf("init socket err\n");
		return -1;
	}
/////////////////////////////////��ʼ��//////////////////////	
	RTMP_LogLevel lvl=RTMP_LOGINFO;
	RTMP_LogSetLevel(lvl);//������Ϣ�ȼ�
	//RTMP_LogSetOutput(FILE*fp);//������Ϣ����ļ�
	rtmp=RTMP_Alloc();//����rtmp�ռ�
	RTMP_Init(rtmp);//��ʼ��rtmp����
	rtmp->Link.timeout=5;//�������ӳ�ʱ����λ�룬Ĭ��30��
	packet= malloc(sizeof(RTMPPacket) ) ;//new RTMPPacket();//������
	RTMPPacket_Alloc(packet,1024*64);//��packet�������ݿռ䣬Ҫ�������֡����֪�������Щ
	RTMPPacket_Reset(packet);//����packet״̬
////////////////////////////////����//////////////////
	RTMP_SetupURL(rtmp,rtmpurl);//����url
	RTMP_EnableWrite(rtmp);//���ÿ�д״̬
	if (!RTMP_Connect(rtmp,NULL))//���ӷ�����
	{
		printf("connect err\n");
		ZCLEAR();
		return -1;
	}
	if (!RTMP_ConnectStream(rtmp,0))//������������(ȡ����rtmp->Link.lFlags)
	{
		printf("ConnectStreamerr\n");
		ZCLEAR();
		return -1;
	}
	packet->m_hasAbsTimestamp = 0; //����ʱ���
	packet->m_nChannel = 0x04; //ͨ��
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
////////////////////////////////////////��������//////////////////////
	while(TRUE)
	{
	/*	int type=0;//����
		int datalength=0;//���ݳ���
		int time=0;//ʱ���
		int streamid=0;//��ID
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
	//////////////////////////////////////////�ͷ�/////////////////////
	if (rtmp!=NULL)
	{
		RTMP_Close(rtmp);//�Ͽ�����
		RTMP_Free(rtmp);//�ͷ��ڴ�
		rtmp=NULL;
	}
	if (packet!=NULL)
	{
		RTMPPacket_Free(packet);//�ͷ��ڴ�
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
