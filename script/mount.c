#include <stdio.h>

int main(int c, char **v)
{
	char suffdisk = 'b';
	char *predisk = "/disk";
	char *premount = "/home/webadm/htdocs/photo2video/upImg";

	int i = 0;
	int j = 0;
	int d = 1;
	int s = 0;

	for (i = 0; i < 100; i++)
	{
		for (j = 0; j < 100; j++)
		{
			for (d = 1; d < 5; d++)
			{
				char mountpoint[256] = {0x0};
				snprintf(mountpoint, sizeof(mountpoint), "%s/d%d/%d/%d", premount, d, i, j);

				for (s = 0; s < 10; s++)
				{
					char diskdir[256] = {0x0};
					snprintf(diskdir, sizeof(diskdir), "%s%c/m2v/photo2video/upImg/d%d/%d/%d", predisk, suffdisk + s, d, i, j);

					char mount[1024] = {0x0};

					snprintf(mount, sizeof(mount), "%s	%s	none  rw,bind 0 0", diskdir, mountpoint);
					fprintf(stdout, "%s\n", mount);
				}
			}
		}
	}
}
