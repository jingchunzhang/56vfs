#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

time_t get_time_t (char *p)
{
	if (strlen(p) != 14)
		return 0;
	struct tm t;
	memset(&t, 0, sizeof(t));
	char b[8] = {0x0};
	snprintf(b, sizeof(b), "%.4s", p);
	t.tm_year = atoi(b) - 1900;
	memset(b, 0, sizeof(b));

	snprintf(b, sizeof(b), "%.2s", p+4);
	t.tm_mon = atoi(b) - 1;
	memset(b, 0, sizeof(b));

	snprintf(b, sizeof(b), "%.2s", p+6);
	t.tm_mday = atoi(b);
	memset(b, 0, sizeof(b));

	snprintf(b, sizeof(b), "%.2s", p+8);
	t.tm_hour = atoi(b);
	memset(b, 0, sizeof(b));

	snprintf(b, sizeof(b), "%.2s", p+10);
	t.tm_min = atoi(b);
	memset(b, 0, sizeof(b));

	snprintf(b, sizeof(b), "%.2s", p+12);
	t.tm_sec = atoi(b);
	memset(b, 0, sizeof(b));

	return mktime(&t);
}

FILE *fpsame;
FILE *fptouch;
FILE *fperr;
FILE *fpdiff;

time_t last ;
time_t end;

int init_log()
{
	fpsame = fopen("./samelog", "w");
	fptouch = fopen("./touchlog", "w");
	fperr = fopen("./errlog", "w");
	fpdiff = fopen("./difflog", "w");

	if (fperr == NULL || NULL == fptouch || NULL == fpsame || NULL == fpdiff)
		return -1;

	return 0;
}

void do_dir(char *indir)
{
	DIR *dp;
	struct dirent *dirp;
	if ((dp = opendir(indir)) == NULL) 
	{
		fprintf(stderr, "add inotify watch opendir %s err %m\n", indir);
		return ;
	}

	char file[256] = {0x0};
	struct stat filestat;
	struct stat filestat2;
	while((dirp = readdir(dp)) != NULL)
	{
		if (dirp->d_name[0] == '.')
			continue;
		snprintf(file, sizeof(file), "%s/%s", indir, dirp->d_name);
		if (stat(file, &filestat))
		{
			fprintf(stderr, "add inotify watch get file stat %s err %m\n", file);
			continue;
		}

		if (S_ISDIR(filestat.st_mode))
		{
			do_dir(file);
			continue;
		}

		int l = strlen(file);
		if (strncmp(file + l - 4, ".flv", 4))
			continue;

		char mp4file[256] = {0x0};
		snprintf(mp4file, sizeof(mp4file), "%s.mp4", file);
		if (stat(mp4file, &filestat2))
		{
			fprintf(fperr, "%s\n", file);
			continue;
		}

		if (filestat2.st_mtime != filestat.st_mtime) 
			fprintf(fpdiff, "touch %s -r %s\n", file, mp4file);
		else if (filestat.st_ctime > last && filestat.st_ctime < end)
			fprintf(fptouch, "touch %s -r %s\n", file, mp4file);
		else
			fprintf(fpsame, "same %s %s\n", file, mp4file);
	}
	closedir(dp);
	return ;
}

int main(int c, char **v)
{
	if (c != 4)
	{
		fprintf(stderr, "Usage %s indir stime etime\n", basename(v[0]));
		return -1;
	}

	last = get_time_t(v[2]);
	end = get_time_t(v[3]);
	if (last == 0 || end == 0)
	{
		fprintf(stderr, "stime %s %s err!\n", v[2], v[3]);
		return -1;
	}

	if (init_log())
		return -1;
	do_dir(v[1]);

	return 0;

}
