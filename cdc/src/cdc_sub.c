static char *csprefix = "cs_voss_";
int cspre_len = 8;

enum {CDC_REALTIME = 0, CDC_PLUS};
enum {F_TYPE_CS = 0, F_TYPE_FCS};

/*
 * 域名：文件名：目标ip：操作类型：任务状态：结束状态：开始时间：当前时间：文件大小：源文件修改时间：文件MD5
 */

static int process_line_fcs(char rec[][256])
{
	if (strcmp(rec[4], "TASK_CLEAN")) 
		return 0;
	t_cdc_data *data;
	char file[256] = {0x0};
	snprintf(file, sizeof(file), "%s:%s", rec[0], rec[1]);
	time_t mtime = atol(rec[12]);
	if (mtime <=0)
		return 0;
	time_t fctime = atol(rec[9]);
	if (fctime <=0)
		return 0;
	if (find_cdc_node(file, &data) == 0)
	{
		t_cdc_val *v = &(data->v);
		if (v->fctime <= fctime)
		{
			snprintf(v->fmd5, sizeof(v->fmd5) + 1, "%s", rec[10]);
			v->fmtime = mtime;
			v->fctime = fctime;
		}
		return 0;
	}

	t_cdc_val v1;
	memset(&v1, 0, sizeof(v1));
	snprintf(v1.fmd5, sizeof(v1.fmd5) + 1, "%s", rec[10]);
	v1.fmtime = mtime;
	v1.fctime = fctime;
	if (add_cdc_node(file, &v1))
	{
		report_2_nm(CDC_ADD_NODE_ERR, ID, LN);
		LOG(fplog, LOG_ERROR, "add_cdc_node %s ERR!\n", file);
		return -1;
	}
	return 0;
}

static int process_line(char *buf, uint32_t ip, int ptype, FILE *fp)
{
	char rec[16][256];
	memset(rec, 0, sizeof(rec));
	int n = sscanf(buf, "%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]", rec[0], rec[1], rec[2], rec[3], rec[4], rec[5], rec[6], rec[7], rec[8], rec[9], rec[10], rec[11], rec[12], rec[13], rec[14], rec[15]);
	if (n < 9)
	{
		LOG(fplog, LOG_ERROR, "err buf:[%s]\n", buf);
		return -1;
	}
	if (strncmp(rec[1], taskfile_prefix, prefix_len))
	{
		LOG(fplog, LOG_ERROR, "err buf:[%s]\n", buf);
		return -1;
	}
	if (atoi(rec[3]))
	{
		LOG(fplog, LOG_NORMAL, "del buf:[%s]\n", buf);
		return 0;
	}
	process_line_fcs(rec);
	if (str2ip(rec[2]) != ip)
	{
		LOG(fplog, LOG_DEBUG, "not self [%s][%u]\n", buf, ip);
		return -1;
	}
	t_cdc_data *data;
	char file[256] = {0x0};
	snprintf(file, sizeof(file), "%s:%s", rec[0], rec[1]);

	if (strcmp(rec[4], "TASK_CLEAN"))
		return 0;

	time_t mtime = atol(rec[7]);
	uint8_t status = CDC_F_UNKNOWN;
	if (atoi(rec[3]))
		status = CDC_F_DEL;
	else 
	{
		if (strcmp(rec[4], "TASK_RUN") == 0)
			status = CDC_F_SYNCING;
		else if (strcmp(rec[5], "OVER_OK") == 0)
			status = CDC_F_OK;
	}

	if (find_cdc_node(file, &data) == 0)
	{
		t_cdc_val *v = &(data->v);
		int i = 0;
		for (i = 0; i < MAX_IP_IN_DIR; i++)
		{
			if (v->ip[i] == ip)
			{
				if (mtime >= v->mtime[i])
				{
					set_n_s(i, status, &(v->status_bists));
					v->mtime[i] = mtime;
				}
				return 0;
			}
			if (v->ip[i] == 0)
			{
				v->ip[i] = ip;
				set_n_s(i, status, &(v->status_bists));
				v->mtime[i] = mtime;
				return 0;
			}
		}
		LOG(fplog, LOG_ERROR, "too may [%s]\n", buf);
		if (fp)
			fprintf(fp, "%s", buf);
		report_2_nm(CDC_TOO_MANY_IP, ID, LN);
		return 1;
	}

	t_cdc_val v1;
	memset(&v1, 0, sizeof(v1));
	v1.ip[0] = ip;
	v1.mtime[0] = mtime;
	set_n_s(0, status, &(v1.status_bists));
	if (add_cdc_node(file, &v1))
	{
		LOG(fplog, LOG_ERROR, "add_cdc_node %s ERR!\n", file);
		report_2_nm(CDC_ADD_NODE_ERR, ID, LN);
		return -1;
	}
	return 0;
}

static void link_fcs_cs_file(char *infile, t_path_info *path, int type)
{
	char bkfile[256] = {0x0};
	snprintf(bkfile, sizeof(bkfile), "%s/%s", path->outdir, basename(infile));
	if (link(infile, bkfile))
		LOG(fplog, LOG_ERROR, "link %s %s error %m!\n", infile, bkfile);

	if (type)
	{
		if (type == 2)
		{
			memset(bkfile, 0, sizeof(bkfile));
			snprintf(bkfile, sizeof(bkfile), "%s/%s", path->bkdir, basename(infile));
			if (rename(infile, bkfile))
				LOG(fplog, LOG_ERROR, "rename %s %s error %m!\n", infile, bkfile);
		}
		else if (unlink(infile))
			LOG(fplog, LOG_ERROR, "link %s %s error %m!\n", infile, bkfile);
		return;
	}

	char workfile[256] = {0x0};
	snprintf(workfile, sizeof(workfile), "%s/%s", path->workdir, basename(infile));
	if (rename(infile, workfile))
		LOG(fplog, LOG_ERROR, "rename %s %s error %m!\n", infile, workfile);
}

static int cdc_sub(t_path_info *path, int type)
{
	DIR *dp;
	struct dirent *dirp;
	char buff[2048];
	char fullfile[256];
	char *indir = path->indir;
	if (type)
		indir = path->workdir;

	if ((dp = opendir(indir)) == NULL) 
	{
		LOG(fplog, LOG_ERROR, "opendir %s error %m!\n", indir);
		return -1;
	}

	FILE *fpin = NULL;

	while((dirp = readdir(dp)) != NULL) {
		if (dirp->d_name[0] == '.')
			continue;

		memset(fullfile, 0, sizeof(fullfile));
		snprintf(fullfile, sizeof(fullfile), "%s/%s", indir, dirp->d_name);
		char *t = strstr(dirp->d_name, csprefix);
		if (!t)
		{
			if (type)
				continue;
			link_fcs_cs_file(fullfile, path, 1);
			continue;
		}
		int ptype = F_TYPE_CS;
		t += cspre_len;
		if (t - dirp->d_name != cspre_len)
			ptype = F_TYPE_FCS;
		char *e = strchr(t, '_');
		if (e == NULL)
		{
			LOG(fplog, LOG_ERROR, "filename err %s\n", dirp->d_name);
			continue;
		}
		*e = 0x0;
		char sip[16] = {0x0};
		snprintf(sip, sizeof(sip), "%s", t);
		*e = '_';

		LOG(fplog, LOG_NORMAL, "process %s\n", fullfile);
		fpin = fopen(fullfile, "r");
		if (fpin == NULL) 
		{
			LOG(fplog, LOG_ERROR, "openfile %s error %m!\n", fullfile);
			continue;
		}
		char hotfile[256] = {0x0};
		snprintf(hotfile, sizeof(hotfile), "%s/h%s", path->tmpdir, dirp->d_name);
		FILE *fphot = fopen(hotfile, "w");
		if (fphot == NULL) 
			LOG(fplog, LOG_ERROR, "openfile %s error %m!\n", hotfile);

		int err = 0;
		uint32_t ip = str2ip(sip);
		memset(buff, 0, sizeof(buff));
		while (fgets(buff, sizeof(buff), fpin))
		{
			LOG(fplog, LOG_DEBUG, "process line:[%s]\n", buff);
			if (process_line(buff, ip, ptype, fphot) > 0)
				err = 1;
			memset(buff, 0, sizeof(buff));
		}
		fclose(fpin);
		if (fphot)
		{
			fclose(fphot);
			if (err)
				link_fcs_cs_file(hotfile, path, 2);
			else
				unlink(hotfile);
		}

		if (type)
			continue;
		link_fcs_cs_file(fullfile, path, 0);
	}
	closedir(dp);
	return 0;
}

