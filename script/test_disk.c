#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

int main(int c, char **v)
{
	if (c != 4)
	{
		fprintf(stderr, "Usage %s block count outfile!\n", v[0]);
		return -1;
	}

	int block = atoi(v[1]);
	int count = atoi(v[2]);

	char *buf = (char *) malloc(block);
	if (buf == NULL)
	{
		fprintf(stderr, "malloc ERROR %m\n");
		return -1;
	}

	int fd = open(v[3], O_CREAT | O_RDWR, 0644);
	if (fd < 0)
	{
		fprintf(stderr, "open %s %m\n", v[3]);
		return -1;
	}

	int i = 0;
	while (i < count)
	{
		if (write(fd, buf, block) != block)
		{
			fprintf(stderr, "wirte %s %m\n", v[3]);
			break;
		}
		i++;
	}
	close(fd);
	free(buf);
	return 0;
}

