/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <stdio.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "log.h"

#define MAX_LOG	50
#define MAX_LOG_LEN	2048
#define DEFAULT_MAX_LOG_NUM 10
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

#define NOWTIME time(NULL)		
#define DELAYCLOSE(num, fd)  delay_close(num, fd, 5)
static char *logcolors[] = {"36m", "35m", "32m", "31m", "41;30m", "41;30m"};
static char *logstring[] = {"TRACE", "DEBUG", "NORMAL", "ERROR", "FAULT", "NONE"};

typedef struct
{
	time_t timeout;
	int fd;
	unsigned char iclose;
	unsigned char bk[3];
}t_fdinfo;

//日志文件对象
struct logentity {
	int fd;				//log fd, internel variable		
	int loglevel;		//log level, caller set
	int rotatesize;		//rotate size(max log file size), caller set, 0 for disable
	unsigned int rotatetime;		//internel variable
	int rotateintval;	//rotate interval time, caller set, 0 for disable	
	int maxlognum;		//maximum log files number, caller set		
	int nextlogno;		//next log file no, internel variable
	char fn[256];		//log filename, caller set
	t_fdinfo fdinfo;    //store delay_close info;
};


static struct logentity logentities[MAX_LOG];
static int curlogid = 0;
#define GETLOGLEVEL(fd) logentities[fd].loglevel

static int openfile(const char* fn) 
{
	return open(fn, O_CREAT | O_RDWR | O_APPEND | O_LARGEFILE, 0644);
}

static void delay_close(int num, int fd, int timeout)
{
	if (logentities[num].fdinfo.iclose)
		close(logentities[num].fdinfo.fd);
	logentities[num].fdinfo.timeout = NOWTIME + timeout;
	logentities[num].fdinfo.iclose = 1;
	logentities[num].fdinfo.fd = fd;
}

static int shiftfile(struct logentity* le) 
{
	char tmp[128] = {0};
	sprintf(tmp, "%s.%d", le->fn, le->nextlogno);
	unlink(tmp);
	rename(le->fn, tmp);
	le->nextlogno = (le->nextlogno + 1) % le->maxlognum;
	if(le->nextlogno == 0)
		le->nextlogno = 1;

	return openfile(le->fn);
}

static int initnextlogno(struct logentity* le) 
{
	char tmp[128] = {0};
	int i;
	struct stat st;
	time_t minmtime = NOWTIME;
	int minno = 1; 
	for(i = 1; i < le->maxlognum; ++i) {
		sprintf(tmp, "%s.%d", le->fn, i);
		if(stat(tmp, &st) != 0)
			break;
		else {
			if(st.st_mtime <= minmtime) {
				minmtime = st.st_mtime;
				minno = i;
			}
		}
	}
	if(i < le->maxlognum)
		return i;
	else
		return minno;
}

static int registerlog_internal(struct logentity* le) 
{
	int ret = pthread_mutex_lock(&log_mutex);
	if (ret != 0)
	{
		fprintf(stderr, "pthread_mutex err %m\n");
		return -1;
	}
	if(curlogid < MAX_LOG - 1) {
		struct logentity* nle = &logentities[curlogid];
		memcpy(nle, le, sizeof(struct logentity));
		nle->fd = openfile(nle->fn);
		if(nle->maxlognum < 2)
			nle->maxlognum = DEFAULT_MAX_LOG_NUM;
		nle->nextlogno = initnextlogno(nle);

		if(nle->fd >= 0) {
			curlogid++;
			if(nle->rotateintval > 0) {
				nle->rotatetime = NOWTIME + nle->rotateintval;
			}
			else {
				nle->rotatetime = 0;
			}
			ret = pthread_mutex_unlock(&log_mutex);
			if (ret != 0)
				fprintf(stderr, "pthread_mutex err %m\n");
			return nle->fd;
		}
	}
	ret = pthread_mutex_unlock(&log_mutex);
	if (ret != 0)
		fprintf(stderr, "pthread_mutex err %m\n");
	return -1;
}

int registerlog(const char* filename, int level, int rotatesize, int rotateintval, int filenum)
{

	struct logentity le;
	memset(&le, 0x0, sizeof(struct logentity));
	strcpy(le.fn, filename);
	le.loglevel = level;
	le.rotatesize = rotatesize << 20;
	le.rotateintval = rotateintval;
	le.maxlognum = filenum;
	
	int ret = registerlog_internal(&le);
	if (ret < 0)
		return ret;
	return curlogid - 1;
}

int getloglevel(const char* llstr) 
{
	if(!strcasecmp(llstr, "trace"))
		return LOG_TRACE;
	else if(!strcasecmp(llstr, "debug"))
		return LOG_DEBUG;
	else if(!strcasecmp(llstr, "normal"))
		return LOG_NORMAL;
	else if(!strcasecmp(llstr, "error"))
		return LOG_ERROR;
	else if(!strcasecmp(llstr, "fault"))
		return LOG_FAULT;
	else
		return LOG_NONE;	
}

void logclose(int fd)
{
	struct logentity* nle = &logentities[fd];
	close(nle->fd);
}

void LOG(int fd, int level, const char* fmt, ...) 
{
	
	if(GETLOGLEVEL(fd) > level)
		return; 
	
	struct logentity* nle = &logentities[fd];
	/* [time string] [thread pid] [log content] */
	int l;
	char buf[MAX_LOG_LEN];
    struct tm tmm; 
	time_t now = NOWTIME;
	localtime_r(&now, &tmm); 
	char *color = logcolors[level];
	char *slog = logstring[level];
	l = snprintf(buf, MAX_LOG_LEN - 1, "\033[%s\033[1m[%s]:[%ld][%04d-%02d-%02d %02d:%02d:%02d]", color, slog, syscall(SYS_gettid), tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday, tmm.tm_hour, tmm.tm_min, tmm.tm_sec);

	va_list ap;
	va_start(ap, fmt);
	l += vsnprintf(buf + l, MAX_LOG_LEN - l - 1, fmt, ap);
	va_end(ap);
	strcat(buf, "\033[0m");
	
	write(nle->fd, buf, l);
}

static void scanlogs() 
{
	struct logentity* le;
	int nfd = -1, ofd, i, shift = 0;
	int now = NOWTIME;
	struct stat64 st;
	
	for(i = 0; i < curlogid; ++i) {
		le = &logentities[i];
		if(le->rotatetime > 0 && le->rotatetime < now) 
		{
			nfd = shiftfile(le);
			if(nfd >= 0) 
			{
				shift = 1;
				le->rotatetime = now + le->rotateintval;
			}
		}

		if(!shift && (le->rotatesize > 0 && !fstat64(le->fd, &st) && le->rotatesize < st.st_size)) 
		{
			nfd = shiftfile(le);
			if(nfd >= 0) 
				shift = 1;
		}

		if(shift) 
		{
			ofd = le->fd;
			le->fd = nfd;
			DELAYCLOSE(i, ofd);
			shift = 0;
		}
	}
}

static void * maintain_log(void *arg)
{
	struct logentity* le;
	time_t cur;
	char th_name[16] = {0x0};
	snprintf(th_name, sizeof(th_name), "log_common");
#ifndef PR_SET_NAME
#define PR_SET_NAME 15
#endif
	prctl(PR_SET_NAME, th_name, 0, 0, 0);
	while (1)
	{
		int i = 0;
		cur = NOWTIME;
		for (i = 0; i < curlogid; i++)
		{
			le = &logentities[i];
			if (le->fdinfo.iclose)
			{
				if (cur >= le->fdinfo.timeout)
				{
					close(le->fdinfo.fd);
					le->fdinfo.iclose = 0;
				}
			}
		}
		scanlogs();
		sleep(10);
	}
}

int init_log()
{
	memset(logentities, 0, sizeof(logentities));
   	pthread_attr_t attr;
   	pthread_t tid;
	int rc;
	pthread_attr_init(&attr);
	if((rc = pthread_create(&tid, &attr, (void*(*)(void*))maintain_log, NULL)) != 0)
	{
	    printf("\7%s: pthread_create(): %m\n", strerror(errno));
	    return -1;
	}
	sleep(1);
	return 0;
}
