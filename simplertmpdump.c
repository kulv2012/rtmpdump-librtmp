//运行指令：  ./simplertmpdump --live -i "rtmp://211.151.86.220:1936/live/hwtest1" -o aaa.mp3 -m 10
#include <unistd.h> 
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <signal.h>		// to catch Ctrl-C
#include <getopt.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "librtmp/rtmp_sys.h"
#include "librtmp/log.h"

#define	SET_BINMODE(f)

#define RD_SUCCESS		0
#define RD_FAILED		1
#define RD_INCOMPLETE		2
#define RD_NO_CONNECT		3

#define DEF_TIMEOUT	30	/* seconds */
#define DEF_BUFTIME	(10 * 60 * 60 * 1000)	/* 10 hours default */

uint32_t nIgnoredFlvFrameCounter = 0;
uint32_t nIgnoredFrameCounter = 0;
#define MAX_IGNORED_FRAMES	50

FILE *file = 0;


char static_str[1024] ;
#define KULV_LOG(fmt, arg...) writelog_in("[pid:%d] [%s] [%s:%d]" fmt "\n", getpid(), getstrtime(static_str, 1024), __FILE__, __LINE__, ## arg)

char *getstrtime(char *t_ime, size_t t_ime_size) {
	time_t tt;
	struct tm vtm;

	time(&tt);
	localtime_r(&tt, &vtm);
	snprintf(t_ime, t_ime_size, "%04d/%02d/%02d %02d:%02d:%02d",vtm.tm_year+1900, vtm.tm_mon+1, vtm.tm_mday, vtm.tm_hour, vtm.tm_min, vtm.tm_sec);
	return t_ime;
}
void writelog_in(  const char *fmt, ... ) {
	char tmpstr[1024] ;

	static int logfd = -2 ;
	if(logfd == -2){
		//logfd = open("./log.simplertmpdump", O_WRONLY|O_APPEND|O_CREAT, 0666) ;
		logfd = open("./log.simplertmpdump", O_WRONLY|O_CREAT|O_APPEND, 0666) ;
	}
	else if( logfd < 0 ){
		logfd = 0 ;
	}

	va_list va;
	va_start(va, fmt); 
	int len = vsnprintf(tmpstr, 1024, fmt, va);
	va_end(va);

	write( logfd, tmpstr, len) ;
	//write( logfd, tmpstr, len) ;
}



void sigIntHandler(int sig)
{
	RTMP_ctrlC = TRUE;
	RTMP_LogPrintf("Caught signal: %d, cleaning up, just a second...\n", sig);
	// ignore all these signals now and let the connection close
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
}


int Download(RTMP * rtmp, FILE * file, int bStdoutMode, int bLiveStream)
{
	int32_t now, lastUpdate;
	int bufferSize = 64 * 1024;
	char *buffer;
	int nRead = 0;
	off_t size = ftello(file);

	rtmp->m_read.timestamp = 0;

	if (rtmp->m_read.timestamp)	{
		RTMP_Log(RTMP_LOGDEBUG, "Continuing at TS: %d ms\n", rtmp->m_read.timestamp);
	}

	if (bLiveStream) {
		RTMP_LogPrintf("Starting Live Stream\n");
	}
	else {
		RTMP_LogPrintf("Starting download at: %.3f kB\n", (double) size / 1024.0);
	}

	rtmp->m_read.initialFrameType = 0;
	rtmp->m_read.nResumeTS = 0;
	rtmp->m_read.metaHeader = 0;
	rtmp->m_read.initialFrame = 0;
	rtmp->m_read.nMetaHeaderSize = 0;
	rtmp->m_read.nInitialFrameSize = 0;

	buffer = (char *) malloc(bufferSize);

	unsigned long lasttimestamp = 0 ;	
	unsigned long streamlasttimestamp = 0 ;	
	now = RTMP_GetTime();
	lastUpdate = now - 1000;
	do
	{
		nRead = RTMP_Read(rtmp, buffer, bufferSize);
		//RTMP_LogPrintf("nRead: %d\n", nRead);
		if (nRead > 0) {
			if (fwrite(buffer, sizeof(unsigned char), nRead, file) != (size_t) nRead) {
				RTMP_Log(RTMP_LOGERROR, "%s: Failed writing, exiting!", __FUNCTION__);
				free(buffer);
				return RD_FAILED;
			}
			size += nRead;

			///////log 
			struct timeval tnow ;
			gettimeofday(&tnow, NULL);
			unsigned long t2_ms = tnow.tv_sec * 1000 + tnow.tv_usec/1000;
			if( lasttimestamp == 0 ) {
				streamlasttimestamp = rtmp->m_read.timestamp ;//这个是音频流里面的时间戳。这个算出来的就是音频的码率
				lasttimestamp = t2_ms ; //这个是我们收到数据的时间戳，这个算出来的是音频的实际接收码率
				continue ;
			}

			float tmprate = (double)8*nRead/1024/( (double)(t2_ms-lasttimestamp)/1000)  ;
			float streamtmprate = (double)8*nRead/1024/( (double)(rtmp->m_read.timestamp - streamlasttimestamp)/1000)  ;
			KULV_LOG("RTMP_Read,ms,nread:%d\trecv timediff:%dms,receive rate:%.2fkbps\tstream timediff:%dms,stream rate:%.1fkbps.", 
					nRead, t2_ms-lasttimestamp, tmprate, rtmp->m_read.timestamp - streamlasttimestamp, streamtmprate );
			lasttimestamp = t2_ms ;
			streamlasttimestamp = rtmp->m_read.timestamp ;

			now = RTMP_GetTime();
			if (abs(now - lastUpdate) > 200) {
				RTMP_LogStatus("\r%.3f kB / %.2f sec. recv speed:%.2f kbps. stream rate:%.2f kbps.", (double) size / 1024.0, (double) (rtmp->m_read.timestamp) / 1000.0, tmprate, streamtmprate);
				lastUpdate = now;
			}
		}
		else {
			RTMP_Log(RTMP_LOGDEBUG, "zero read!");
			if (rtmp->m_read.status == RTMP_READ_EOF)
				break;
		}

	}
	while (!RTMP_ctrlC && nRead > -1 && RTMP_IsConnected(rtmp) && !RTMP_IsTimedout(rtmp));

	free(buffer);
	if (nRead < 0)
		nRead = rtmp->m_read.status;

	RTMP_Log(RTMP_LOGDEBUG, "RTMP_Read returned: %d", nRead);

	if (nRead == -3)
		return RD_SUCCESS;

	if ( RTMP_ctrlC || nRead < 0 || RTMP_IsTimedout(rtmp)) {
		return RD_INCOMPLETE;
	}

	return RD_SUCCESS;
}

#define STR2AVAL(av,str)	av.av_val = str; av.av_len = strlen(av.av_val)

void usage(char *prog)
{
	RTMP_LogPrintf
		("\n%s: This program dumps the media content streamed over RTMP.\n\n", prog);
	RTMP_LogPrintf("--help|-h               Prints this help screen.\n");
	RTMP_LogPrintf
		("--url|-i url            URL with options included (e.g. rtmp://host[:port]/path swfUrl=url tcUrl=url)\n");
	RTMP_LogPrintf
		("--rtmp|-r url           URL (e.g. rtmp://host[:port]/path)\n");
	RTMP_LogPrintf
		("--live|-v               Save a live stream, no --resume (seeking) of live streams possible\n");
	RTMP_LogPrintf
		("--flv|-o string         FLV output file name, if the file name is - print stream to stdout\n");
	RTMP_LogPrintf
		("--timeout|-m num        Timeout connection num seconds (default: %u)\n", DEF_TIMEOUT);
	RTMP_LogPrintf
		("--quiet|-q              Suppresses all command output.\n");
	RTMP_LogPrintf("--verbose|-V            Verbose command output.\n");
	RTMP_LogPrintf
		("If you don't pass parameters for swfUrl, pageUrl, or auth these properties will not be included in the connect ");
	RTMP_LogPrintf("packet.\n\n");
}

int main(int argc, char **argv)
{
	extern char *optarg;

	int nStatus = RD_SUCCESS;
	int bStdoutMode = TRUE;	// if true print the stream directly to stdout, messages go to stderr
	int bLiveStream = TRUE;	// is it a live stream? then we can't seek/resume

	long int timeout = DEF_TIMEOUT;	// timeout connection after 120 seconds
	RTMP rtmp = { 0 };
	AVal fullUrl = { 0, 0 };
	RTMP_debuglevel = RTMP_LOGINFO;
	char *flvFile = 0;

	signal(SIGINT, sigIntHandler);
	signal(SIGTERM, sigIntHandler);
	signal(SIGHUP, sigIntHandler);
	signal(SIGPIPE, sigIntHandler);
	signal(SIGQUIT, sigIntHandler);


	RTMP_LogPrintf("RTMPDump %s\n", RTMPDUMP_VERSION);
	RTMP_LogPrintf("(c) 2010 Andrej Stepanchuk, Howard Chu, The Flvstreamer Team; license: GPL\n");


	int opt;
	struct option longopts[] = {
		{"help", 0, NULL, 'h'},
		{"url", 1, NULL, 'i'},
		{"rtmp", 1, NULL, 'r'},
		{"live", 0, NULL, 'v'},
		{"timeout", 1, NULL, 'm'},
		{"hashes", 0, NULL, '#'},
		{"quiet", 0, NULL, 'q'},
		{"verbose", 0, NULL, 'V'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "hVvqzRr:s:t:i:p:a:b:f:o:u:C:n:c:l:y:Ym:k:d:A:B:T:w:x:W:X:S:#j:", longopts, NULL)) != -1)
	{
		switch (opt)
		{
			case 'h':
				usage(argv[0]);
				return RD_SUCCESS;

			case 'v':
				bLiveStream = TRUE;	// no seeking or resuming possible!
				break;
			case 'i':
				STR2AVAL(fullUrl, optarg);
				break;
			case 'o':
				flvFile = optarg;
				if (strcmp(flvFile, "-"))
					bStdoutMode = FALSE;

				break;
			case 'm':
					  timeout = atoi(optarg);
					  break;
			case 'q':
					  RTMP_debuglevel = RTMP_LOGCRIT;
					  break;
			case 'V':
					  RTMP_debuglevel = RTMP_LOGDEBUG;
					  break;
			case 'z':
					  RTMP_debuglevel = RTMP_LOGALL;
					  break;
			default:
					  RTMP_LogPrintf("unknown option: %c\n", opt);
					  usage(argv[0]);
					  return RD_FAILED;
					  break;
		}
	}

	if (flvFile == 0) {
		RTMP_Log(RTMP_LOGWARNING, "You haven't specified an output file (-o filename), using stdout");
		bStdoutMode = TRUE;
	}
	if (!file)	{
		if (bStdoutMode) {
			file = stdout;
			SET_BINMODE(file);
		}
		else {
			file = fopen(flvFile, "w+b");
			if (file == 0)	{
				RTMP_LogPrintf("Failed to open file! %s\n", flvFile);
				return RD_FAILED;
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////
	RTMP_Init(&rtmp);//初始化RTMP参数
	//指定了-i 参数，直接设置URL
	if (RTMP_SetupURL(&rtmp, fullUrl.av_val) == FALSE) {
		RTMP_Log(RTMP_LOGERROR, "Couldn't parse URL: %s", fullUrl.av_val);
		return RD_FAILED;
	}

	rtmp.Link.timeout = timeout ;
	/* Try to keep the stream moving if it pauses on us */
	if (!bLiveStream )
		rtmp.Link.lFlags |= RTMP_LF_BUFX;
	else {
		rtmp.Link.lFlags |= RTMP_LF_LIVE ;
	}

	printf("aaaaa:%d\n", rtmp.Link.lFlags ) ;
	while (!RTMP_ctrlC)
	{
		RTMP_Log(RTMP_LOGDEBUG, "Setting buffer time to: %dms", DEF_BUFTIME);
		RTMP_SetBufferMS(&rtmp, 2000);//告诉服务器帮我缓存多久

		RTMP_LogPrintf("Connecting ...\n");
		if (!RTMP_Connect(&rtmp, NULL))	{//建立连接,发送"connect"
			nStatus = RD_NO_CONNECT;
			break;
		}
		RTMP_Log(RTMP_LOGINFO, "Connected...");

		//处理服务端返回的各种控制消息包，比如收到connect的result后就进行createStream，以及发送play(test)消息
		if (!RTMP_ConnectStream(&rtmp, 0)) {//一旦返回，表示服务端开始发送数据了.可以play了
			nStatus = RD_FAILED;
			break;
		}

		nStatus = Download(&rtmp, file, bStdoutMode, bLiveStream );

		if (nStatus != RD_INCOMPLETE || !RTMP_IsTimedout(&rtmp) || bLiveStream)
			break;
	}

	RTMP_LogPrintf("Download complete\n");

//clean:
	RTMP_Log(RTMP_LOGDEBUG, "Closing connection.\n");
	RTMP_Close(&rtmp);

	if (file != 0)
		fclose(file);


	return nStatus;
}
