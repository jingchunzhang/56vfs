#include <stdio.h>   
#include <unistd.h>   
#include <sys/select.h>   
#include <errno.h>   
#include <sys/inotify.h>   
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_WATCH 102400

const char *type[30] = {"nothing", "access", "modify", "attrib", "close_write", "close_nowrite", "open", "move_from", "move_to", "create", "delete"};

int gettype(unsigned int mask)
{
	int index = 1;
	while (mask != IN_ACCESS)
	{
		mask = mask >> 1;
		index++;
	}
	return index;
}

char watch_dirs[MAX_WATCH][256];

static void   _inotify_event_handler(struct inotify_event *event)      //从buf中取出一个事件。  
{
	int len = strlen(event->name);
	if (len < 2)
		return;

	char *dir = watch_dirs[event->wd];
	int index = gettype(event->mask);
	printf("%d:event->Mask: [%08X]%s[%08X]\n", __LINE__, event->mask, type[index], event->cookie);   
	printf("%d:event->Name: %s/%s:%d\n", __LINE__, dir, event->name, event->wd);   
}   

static int add_watch(int fd, char *indir, unsigned int mask)
{
	int wd = inotify_add_watch(fd, indir, mask);
	if (wd == -1)
	{
		fprintf(stderr, "inotify_add_watch %s err %m\n", indir);
		return -1;
	}
	if (wd >= MAX_WATCH)
	{
		fprintf(stderr, "too large wd %d\n", wd);
		return -1;
	}
	snprintf(watch_dirs[wd], sizeof(watch_dirs[wd]), "%s", indir);
	DIR *dp;
	struct dirent *dirp;
	if ((dp = opendir(indir)) == NULL) 
	{
		fprintf(stderr, "opendir %s err %m\n", indir);
		return -1;
	}

	int ret = 0;
	char file[256] = {0x0};
	struct stat filestat;
	while((dirp = readdir(dp)) != NULL)
	{
		if (dirp->d_name[0] == '.')
			continue;
		snprintf(file, sizeof(file), "%s/%s", indir, dirp->d_name);
		if (stat(file, &filestat))
		{
			fprintf(stderr, "stat %s err %m\n", file);
			ret = -1;
			break;
		}

		if (S_ISDIR(filestat.st_mode))
		{
			if (add_watch(fd, file, mask))
				return -1;
		}
	}
	closedir(dp);
	return ret;
}

int  main(int argc, char **argv)   
{   
	if (argc != 2) 
	{   
		printf("Usage: %s <file/dir>\n", argv[0]);   
		return -1;   
	}   
	memset(watch_dirs, 0, sizeof(watch_dirs));

	unsigned char buf[1024] = {0};   
	struct inotify_event *event = NULL;              

	int fd = inotify_init();
	uint32_t mask = IN_CLOSE_WRITE|IN_CREATE|IN_DELETE|IN_MODIFY|IN_MOVED_FROM|IN_MOVED_TO;
	int ret = add_watch(fd, argv[1], mask);
	if (ret)
		return ret;

	FILE *fp = fopen("./test.log", "w");

	for (;;) 
	{   
		fd_set fds;   
		FD_ZERO(&fds);                
		FD_SET(fd, &fds);   

		if (select(fd + 1, &fds, NULL, NULL, NULL) > 0)
		{   
			int len, index = 0;   
			while (((len = read(fd, &buf, sizeof(buf))) < 0) && (errno == EINTR));       //没有读取到事件。
			while (index < len) 
			{   
				event = (struct inotify_event *)(buf + index);                       
				_inotify_event_handler(event);                                             //获取事件。
				index += sizeof(struct inotify_event) + event->len;             //移动index指向下一个事件。
			}   
		}   
	}   

	//inotify_rm_watch(fd, wd);              //删除对指定文件的监控。

	return 0;   
}
