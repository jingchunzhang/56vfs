#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <time.h>
#include <sys/types.h>
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

int main(int c, char **v)
{
	if (c != 3)
	{
		fprintf(stderr, "Usage %s infile stime!\n", basename(v[0]));
		return -1;
	}

	time_t last = get_time_t(v[2]);
	if (last == 0)
	{
		fprintf(stderr, "stime %s err!\n", v[2]);
		return -1;
	}
	fprintf(stdout, "%u\n", last);

	FILE *fp = fopen(v[1], "r");
	if (fp == NULL)
	{
		fprintf(stderr, "fopen err %m\n");
		return -1;
	}

	int i = 0;
	struct stat filestat;
	char buf[256] = {0x0};
	while (fgets(buf, sizeof(buf), fp))
	{
		char *t = strrchr(buf, '\n');
		if (t)
			*t = 0x0;
		if (stat(buf, &filestat))
		{
			fprintf(stderr, "stat %s err %m\n", buf);
			memset(buf, 0, sizeof(buf));
			continue;
		}
		if (filestat.st_ctime > last)
			i++;
		memset(buf, 0, sizeof(buf));
	}
	fclose(fp);
	fprintf(stdout, "%d\n", i);
	return 0;

}
