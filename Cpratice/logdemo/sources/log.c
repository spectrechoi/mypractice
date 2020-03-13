/*log.c by cai browser in csdn*/
#include <unistd.h>
#include "./include/log.h"

//path
#define filepath "./ps_com_log.log"
#define USE_SUCESS		0
//set time
static unsigned char  settime(char *time_s)
{
	time_t timer = time(NULL);
	strftime(time_s, 20, "%Y-%m-%d %H:%M%S", localtime(&timer));

	return USE_SUCESS;
}

//print

static int printflog(char *logText,char *string)
{
	FILE *fd = NULL;
	char s[1024];
	char tmp[256];

	fd = fopen(filepath,"a+");
	if(NULL == fd){
		return -1;
	}

	memset(s, 0, sizeof(s));
	memset(tmp, 0, sizeof(tmp));

	sprintf(tmp, "****[pid=%d]:[",getpid());
	strcpy(s,tmp);

	memset(tmp, 0, sizeof(tmp));
	settime(tmp);
	strcat(s,tmp);
	
	strcat(s,"]****)");
	fprintf(fd,"%s",s);

	
	fprintf(fd,"*[%s]*****:\n",logText);
	fprintf(fd,"%s\n",string);
	fclose(fd);
}

void LogWrite(char *logText,char *string)
{
	//[为支持多线程需要加锁] pthread_mutex_lock(&mutex_log); //lock. 
	//打印日志信息
	printflog(logText, string);
																		
	//[为支持多线程需要加锁] pthread_mutex_unlock(&mutex_log); //unlock. 
															
}




