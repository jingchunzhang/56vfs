
static void process_db(char rec[][256], char *sip, int role)
{
	t_voss_key k;
	t_voss_val v;
	memset(&k, 0, sizeof(k));
	memset(&v, 0, sizeof(v));

	if (role == ROLE_FCS)
		snprintf(k.ip, sizeof(k.ip), "%s", rec[0]);
	else
		snprintf(k.ip, sizeof(k.ip), "%s", sip);
	snprintf(k.domain, sizeof(k.domain), "%s", rec[0]);
	snprintf(k.fname, sizeof(k.fname), "%s", rec[1]);
	snprintf(k.task_type, sizeof(k.task_type), "%s", rec[3]);
	time_t stime = atoll(rec[6]) > 0 ? atoll(rec[6]) : atoll(rec[9]);
	get_strtime_by_t(k.task_stime, stime);

	v.fsize = atol(rec[8]);
	snprintf(v.fmd5, sizeof(v.fmd5), "%s", rec[10]);
	snprintf(v.over_status, sizeof(v.over_status), "%s", rec[5]);
	get_strtime_by_t(v.task_ctime, atoll(rec[7]));
	snprintf(v.role, sizeof(v.role), "%d", role);
	mydb_get_voss(&k, &v);
}

static int process_line(char *buf, int role, char *sip)
{
	char rec[16][256];
	memset(rec, 0, sizeof(rec));
	int n = sscanf(buf, "%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]", rec[0], rec[1], rec[2], rec[3], rec[4], rec[5], rec[6], rec[7], rec[8], rec[9], rec[10], rec[11], rec[12], rec[13], rec[14], rec[15]);
	if (n < 9)
	{
		LOG(cdc_db_log, LOG_ERROR, "err buf:[%s]\n", buf);
		return -1;
	}
	if (strncmp(rec[1], "/home", 5))
		return 0;
	if (strcmp(rec[4], "TASK_CLEAN"))
		return 0;
	uint32_t ip = str2ip(sip);;
	uint32_t dstip = str2ip(rec[2]);
	if (role == ROLE_CS && dstip != ip)
		return 0;

	process_db(rec, sip, role);
	return 0;
}

static int process_stat(char *buf)
{
	char rec[16][256];
	memset(rec, 0, sizeof(rec));
	int n = sscanf(buf, "%[^|]|%[^|]|%[^|]|%[^|]|", rec[0], rec[1], rec[2], rec[3]);
	if (n < 3)
	{
		LOG(cdc_db_log, LOG_ERROR, "err buf:[%s]\n", buf);
		return -1;
	}
	if (rec[2][0] < 'A' || rec[2][0] > 'Z')
		return 0;
	process_db_stat(rec);
	return 0;
}

static int do_bk_file(char *file, char *day, int type)
{
	char bkdir[256] = {0x0};
	snprintf(bkdir, sizeof(bkdir), "%s/%s", cdc_path.bkdir, day);
	if (mkdir(bkdir, 0755) && errno != EEXIST)
	{
		LOG(cdc_db_log, LOG_ERROR, "mkdir %s err %m\n", bkdir);
		return -1;
	}

	char bkfile[256] = {0x0};
	/*
	if (type == ROLE_HOT_CS)
	{
		snprintf(bkfile, sizeof(bkfile), "%s/%s", redisdir, basename(file));
		if(link(file, bkfile))
			LOG(cdc_db_log, LOG_ERROR, "link %s to %s err %m\n", file, bkfile);
		memset(bkfile, 0, sizeof(bkfile));
	}
	*/
	snprintf(bkfile, sizeof(bkfile), "%s/%s", bkdir, basename(file));

	if(rename(file, bkfile))
	{
		LOG(cdc_db_log, LOG_ERROR, "rename %s to %s err %m\n", file, bkfile);
		return -1;
	}
	return 0;
}

static int get_ip_day_from_file (char *file, char *sip, char *day)
{
	char *t = strstr(file, "voss_stat_");
	if (t)
	{
		t += 10;
		snprintf(day, 16, "%.8s", t);
		return 0;
	}
	t = strstr(file, "voss_");
	if (t == NULL)
		return -1;
	t += 5;
	char *e = strchr(t, '_');
	if (e == NULL)
		return -1;
	*e = 0x0;
	strcpy(sip, t); 
	*e = '_';
	e++;
	t = strchr(e, '.');
	if (t == NULL)
		return -1;
	*t = 0x0;
	snprintf(day, 16, "%.8s", e);
	*t = '.';
	return 0;
}

int do_refresh_run_task()
{
	DIR *dp;
	struct dirent *dirp;
	char buff[2048];
	char fullfile[256];

	if ((dp = opendir(cdc_path.indir)) == NULL) 
	{
		LOG(cdc_db_log, LOG_ERROR, "opendir %s error %m!\n", cdc_path.indir);
		return -1;
	}

	FILE *fpin = NULL;
	while((dirp = readdir(dp)) != NULL) {
		if (dirp->d_name[0] == '.')
			continue;
		memset(fullfile, 0, sizeof(fullfile));
		snprintf(fullfile, sizeof(fullfile), "%s/%s", cdc_path.indir, dirp->d_name);
		int role = UNKOWN;
		if (dirp->d_name[0] == 'c')
			role = ROLE_CS;
		else if (dirp->d_name[0] == 'f')
			role = ROLE_FCS;
		else if (dirp->d_name[0] == 't')
			role = ROLE_TRACKER;
		else if (dirp->d_name[0] == 'v')
			role = ROLE_VOSS_MASTER;
		else if (dirp->d_name[0] == 'h')
			role = ROLE_HOT_CS;

		if (role == ROLE_VOSS_MASTER)
			continue;

		char sip[16] = {0x0};
		char day[16] = {0x0};
		if (get_ip_day_from_file(dirp->d_name, sip, day))
		{
			LOG(cdc_db_log, LOG_ERROR, "file name %s err\n", fullfile);
			role = UNKOWN;
		}

		char curtime[16] = {0x0};
		get_strtime(curtime);
		if(strncmp(curtime, day, 8))
		{
			if (process_curday)
				continue;
		}
		else 
		{
			if (process_curday == 0)
				continue;
		}
		if (role != UNKOWN)
		{
			LOG(cdc_db_log, LOG_NORMAL, "process %s\n", fullfile);
			fpin = fopen(fullfile, "r");
			if (fpin == NULL) 
			{
				LOG(cdc_db_log, LOG_ERROR, "openfile %s error %m!\n", fullfile);
				continue;
			}
			memset(buff, 0, sizeof(buff));
			while (fgets(buff, sizeof(buff), fpin))
			{
				LOG(cdc_db_log, LOG_TRACE, "process line:[%s]\n", buff);
				if (ROLE_VOSS_MASTER != role)
					process_line(buff, role, sip);
				else
					process_stat(buff);
				memset(buff, 0, sizeof(buff));
			}
			fclose(fpin);
		}
		do_bk_file(fullfile, day, role);
	}
	closedir(dp);
	return 0;
}
