#define shmfile "shmfile_"

typedef struct
{
	time_t ftime;
	char fname[256];
}shmfileinfo;

static int sortfile(const void *p1, const void *p2)
{
	shmfileinfo* h1 = (shmfileinfo *) p1;
	shmfileinfo* h2 = (shmfileinfo *) p2;
	return h2->ftime > h1->ftime;
}

static int get_last_sync_file(t_path_info *path, char *file, int len)
{
	DIR *dp;
	struct dirent *dirp;
	char fullfile[256];

	int ret = -1;
	if ((dp = opendir(path->fulldir)) == NULL) 
	{
		LOG(fplog, LOG_ERROR, "opendir %s error %m!\n", path->fulldir);
		report_2_nm(CDC_NORMAL_ERR, ID, LN);
		return ret;
	}

	int shmfilelen = strlen(shmfile);
	time_t last = 0;
	while((dirp = readdir(dp)) != NULL) 
	{
		if (dirp->d_name[0] == '.')
			continue;
		if (strncmp(dirp->d_name, shmfile, shmfilelen))
			continue;

		memset(fullfile, 0, sizeof(fullfile));
		snprintf(fullfile, sizeof(fullfile), "%s/%s", path->fulldir, dirp->d_name);
		struct stat filestat;
		if (stat(fullfile, &filestat) < 0)
		{
			LOG(fplog, LOG_ERROR, "stat %s error %m!\n", fullfile);
			report_2_nm(CDC_NORMAL_ERR, ID, LN);
			continue;
		}
		if (last < filestat.st_mtime)
		{
			last = filestat.st_mtime;
			memset(file, 0, len);
			snprintf(file, len, "%s", fullfile);
			ret = 0;
		}
	}
	closedir(dp);
	return ret;
}

static int clear_plus(t_path_info *path)
{
	DIR *dp;
	struct dirent *dirp;
	char fullfile[256];

	if ((dp = opendir(path->workdir)) == NULL) 
	{
		LOG(fplog, LOG_ERROR, "opendir %s error %m!\n", path->workdir);
		report_2_nm(CDC_NORMAL_ERR, ID, LN);
		return -1;
	}
	while((dirp = readdir(dp)) != NULL) 
	{
		if (dirp->d_name[0] == '.')
			continue;

		memset(fullfile, 0, sizeof(fullfile));
		snprintf(fullfile, sizeof(fullfile), "%s/%s", path->workdir, dirp->d_name);
		char *t = strstr(dirp->d_name, csprefix);
		if (!t)
			continue;
		if (unlink(fullfile))
		{
			LOG(fplog, LOG_ERROR, "unlink %s err %m\n", fullfile);
			report_2_nm(CDC_NORMAL_ERR, ID, LN);
			continue;
		}
	}
	closedir(dp);

	return 0;
}

static int mini_fullfile(t_path_info *path)
{
	int max = myconfig_get_intval("shm_fullfile_count", 3);
	if (max >= 10)
		max = 10;

	DIR *dp;
	struct dirent *dirp;
	char fullfile[256];
	if ((dp = opendir(path->fulldir)) == NULL) 
	{
		LOG(fplog, LOG_ERROR, "opendir %s error %m!\n", path->fulldir);
		report_2_nm(CDC_NORMAL_ERR, ID, LN);
		return -1;
	}

	shmfileinfo finfo[64];
	memset(finfo, 0, sizeof(finfo));
	shmfileinfo *pfinfo = finfo;
	int count = 0;
	int shmfilelen = strlen(shmfile);
	while((dirp = readdir(dp)) != NULL) 
	{
		if (dirp->d_name[0] == '.')
			continue;
		if (strncmp(dirp->d_name, shmfile, shmfilelen))
			continue;

		memset(fullfile, 0, sizeof(fullfile));
		snprintf(fullfile, sizeof(fullfile), "%s/%s", path->fulldir, dirp->d_name);
		struct stat filestat;
		if (stat(fullfile, &filestat) < 0)
		{
			LOG(fplog, LOG_ERROR, "stat %s error %m!\n", fullfile);
			report_2_nm(CDC_NORMAL_ERR, ID, LN);
			continue;
		}
		pfinfo->ftime = filestat.st_ctime;
		snprintf(pfinfo->fname, sizeof(pfinfo->fname), "%s", fullfile);
		pfinfo++;
		count++;
	}
	closedir(dp);
	if (count <= max)
		return 0;
	qsort(finfo, count, sizeof(shmfileinfo), sortfile);
	pfinfo = finfo + max;
	int i = max;
	for (; i < count; i++)
	{
		if (unlink(pfinfo->fname) < 0)
		{
			LOG(fplog, LOG_ERROR, "unlink %s error %m!\n", pfinfo->fname);
			report_2_nm(CDC_NORMAL_ERR, ID, LN);
			continue;
		}
	}
	return 0;
}

static int do_sync_2_disk(t_path_info *path)
{
	char tmpfile[256] = {0x0};
	char outfile[256] = {0x0};
	char stime[16] = {0x0};
	get_strtime(stime);
	snprintf(tmpfile, sizeof(tmpfile), "%s/tmp_%s%s", path->fulldir, shmfile, stime); 
	snprintf(outfile, sizeof(outfile), "%s/%s%s", path->fulldir, shmfile, stime); 

	if (cdc_sync_data(tmpfile))
	{
		LOG(fplog, LOG_ERROR, "cdc_sync_data err %s %m\n", tmpfile);
		report_2_nm(CDC_NORMAL_ERR, ID, LN);
		return -1;
	}

	if (rename(tmpfile, outfile))
	{
		LOG(fplog, LOG_ERROR, "rename %s to %s err %m\n", tmpfile, outfile);
		report_2_nm(CDC_NORMAL_ERR, ID, LN);
		return -1;
	}

	mini_fullfile(path);

	return clear_plus(path);
}

