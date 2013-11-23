/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#define MONTH_SECOND 2592000

static void add_a_valid_ip(uint32_t tip, uint32_t ip[MAX_IP_IN_DIR])
{
	int i = 0; 
	for ( i = 0; i < MAX_IP_IN_DIR; i++)
	{
		if (ip[i] == tip)
			return;
		if (ip[i] == 0)
		{
			ip[i] = tip;
			return;
		}
	}
}

static int get_srcip(char *dstip, t_ip_isp * isps, char *srcip, char *f)
{
	t_ip_info ipinfo0;
	t_ip_info *ipinfo = &ipinfo0;
	if (get_ip_info(&ipinfo, dstip, 1))
	{
		LOG(vfs_http_log, LOG_ERROR, "get_ip_info err %s\n", dstip);
		return -1;
	}
	uint32_t udstip = str2ip(dstip);

	uint32_t ip[MAXISP][MAX_IP_IN_DIR];
	memset(ip, 0, sizeof(ip));
	t_valid_isp *isp = &(valid_isp[ipinfo->isp%MAXISP]);
	int j = 0;
	for (j = 0; j < MAX_IP_IN_DIR; j++)
	{
		if (strlen(isps->ip) == 0)
			break;
		add_a_valid_ip(str2ip(isps->ip), ip[isps->isp%MAXISP]);
		isps++;
	}

	t_cs_dir_info cs;
	memset(&cs, 0, sizeof(cs));
	if (get_cs_info_by_path(f, &cs))
		LOG(vfs_http_log, LOG_ERROR, "get_cs_info_by_path err %s \n", f);
	else
	{
		for (j = 0; j < cs.index; j++)
		{
			if (cs.ip[j] == 0)
				break;
			if (udstip == cs.ip[j])
				continue;
			add_a_valid_ip(cs.ip[j], ip[cs.isp[j]%MAXISP]);
		}
	}
	return select_ip(ip, srcip, isp);
}

static void do_check_ip(char *fc, t_ip_isp * isps, char **fmd5)
{
	t_cdc_data *d = NULL;
	if (find_cdc_node(fc, &d))
	{
		LOG(vfs_http_log, LOG_ERROR, "find_cdc_node %s no result, agent will try src!\n", fc);
		*fmd5 = defualtmd5;
		return ;
	}
	t_cdc_val *v = &(d->v);
	*fmd5 = v->fmd5;
	*fmd5 = defualtmd5;
	t_ip_info ipinfo0;
	t_ip_info *ipinfo = &ipinfo0;
	int j = 0;
	for (j = 0; j < MAX_IP_IN_DIR; j++)
	{
		memset(isps, 0, sizeof(t_ip_isp));
		if (v->ip[j] == 0)
			continue;
		int s;
		get_n_s(j, &s, &(v->status_bists));
		if (s != CDC_F_OK)
			continue;
		ip2str(isps->ip, v->ip[j]);
		if (get_ip_info(&ipinfo, isps->ip, 1))
			LOG(vfs_http_log, LOG_ERROR, "get_ip_info err in shm %s\n", isps->ip);
		else
		{
			isps->isp = ipinfo->isp;
			isps++;
		}
	}
}

static int get_hot_ips(int type, char *f, t_ips *ips)
{
	if (type == CNC || TEL == type)
	{
		if (get_cfg_lock())
		{
			LOG(vfs_http_log, LOG_ERROR, "get_hot_ips err %d, try later!\n", type);
			return -1; 
		}
		t_ip_info_list *server = NULL;
		list_head_t *l;
		list_for_each_entry_safe_l(server, l, &hothome, hotlist)
		{
			if (server->ipinfo.isp == type)
			{
				snprintf(ips->ip, sizeof(ips->ip), "%s", server->ipinfo.sip);
				LOG(vfs_http_log, LOG_NORMAL, "hotfile %s %s\n", ips->ip, ispname[type]);
				ips++;
			}
		}
		release_cfg_lock();
		return 0;
	}

	t_cs_dir_info cs;
	memset(&cs, 0, sizeof(cs));
	if (get_cs_info_by_path(f, &cs))
	{
		LOG(vfs_http_log, LOG_ERROR, "get_cs_info_by_path err %s %m\n", f);
		return -1; 
	}
	int i = 0;
	for (i = 0; i < cs.index; i++)
	{
		if (cs.isp[i] != type)
			continue;
		ip2str(ips->ip, cs.ip[i]);
		LOG(vfs_http_log, LOG_NORMAL, "hotfile %s %s\n", ips->ip, ispname[type]);
		ips++;
	}
	return 0;
}

static int get_hot_ips_uint(uint32_t *ips)
{
	if (get_cfg_lock())
	{
		LOG(vfs_http_log, LOG_ERROR, "get_hot_ips err  try later!\n");
		return -1; 
	}
	t_ip_info_list *server = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(server, l, &hothome, hotlist)
	{
		*ips = server->ipinfo.ip;
		ips++;
	}
	release_cfg_lock();
	return 0;
}

static int get_isp_ips_uint(uint8_t isp, uint32_t *ips, uint8_t type)
{
	if (get_cfg_lock())
	{
		LOG(vfs_http_log, LOG_ERROR, "get_isp_ips err  try later!\n");
		return -1; 
	}

	list_head_t *hlist = &(isp_iplist[isp&MAXISP]);
	t_ip_info_list *server = NULL;
	list_head_t *l;

	if (type == 0)
	{
		list_for_each_entry_safe_l(server, l, hlist, isplist)
		{
			if (server->ipinfo.isp != isp)
				continue;
			*ips = server->ipinfo.ip;
			ips++;
		}
	}
	else
	{
		list_for_each_entry_safe_l(server, l, hlist, archive_list)
		{
			if (server->ipinfo.archive_isp != isp)
				continue;
			*ips = server->ipinfo.ip;
			ips++;
		}
	}
	release_cfg_lock();
	return 0;
}

static int check_self_in_shm(char *s, t_ip_isp *isps, char *f, char *d)
{
	return -1;
	int i = 0;
	for (i = 0; i < MAX_IP_IN_DIR; i++)
	{
		if(strcmp(s, isps->ip) == 0)
		{
			LOG(vfs_http_log, LOG_NORMAL, "%s have %s:%s now!\n", s, d, f);
			return 0;
		}
		isps++;
	}
	if (check_hotip_task(s, f, d) == 0)
	{
		LOG(vfs_http_log, LOG_NORMAL, "%s have %s:%s now in db!\n", s, d, f);
		return 0;
	}
	return -1;
}

static void do_hot_sync(int type, t_sync_file *p, char *o, int *ol)
{
	char srcip[16] = {0x0};
	int l = 0;
	char subbuf[1024];
	t_ips ips[2048];
	memset(ips, 0, sizeof(ips));
	if (get_hot_ips(type, p->f, ips))
	{
		LOG(vfs_http_log, LOG_ERROR, "hotfile %s get_hot_ips err\n", p->fc);
		return;
	}
	t_ip_isp isps[MAX_IP_IN_DIR];
	memset(isps, 0, sizeof(isps));
	char *fmd5 = NULL;
	do_check_ip(p->fc, isps, &fmd5);
	int i = 0;
	for (i = 0; i < 2048; i++)
	{
		if (strlen(ips[i].ip) == 0)
			break;
		if (check_self_in_shm(ips[i].ip, isps, p->f, p->d) == 0)
			continue;
		if (check_dup_task(ips[i].ip, p->d, p->f) == 0)
			continue;
		memset(srcip, 0, sizeof(srcip));
		memset(subbuf, 0, sizeof(subbuf));
		if (type == MP4) 
			l = snprintf(subbuf, sizeof(subbuf), "iplist=%s&vfs_cmd=M_SYNCFILE&%s=%s\n", ips[i].ip, p->d, p->f);
		else if (get_srcip(ips[i].ip, isps, srcip, p->f) == 0)
		{
			if (add_dup_task(ips[i].ip, p->d, p->f))
			{
				LOG(vfs_http_log, LOG_ERROR, "add_dup_task err %s\n", p->f);
				return;
			}
			l = snprintf(subbuf, sizeof(subbuf), "iplist=%s&vfs_cmd=M_SYNCFILE&%s=%s,%s,%s\n", ips[i].ip, p->d, p->f, srcip, fmd5);
		}
		else
		{
			LOG(vfs_http_log, LOG_ERROR, "%s ip %s get_srcip error\n", p->fc, ips[i].ip);
			continue;
		}
		if (l + *ol >= SENDBUFSIZE)
		{
			LOG(vfs_http_log, LOG_ERROR, "hotfile %s too long ip %d:%d:%d\n", p->fc, l, *ol, SENDBUFSIZE);
			return;
		}
		*ol += snprintf(o + *ol, SENDBUFSIZE - *ol, "%s", subbuf);
	}
}

static void do_syncfile_iplist(char *s, t_sync_file *p, char *o, int *ol)
{
	char srcip[16] = {0x0};
	int l = 0;
	char subbuf[1024];
	t_ip_isp isps[MAX_IP_IN_DIR];
	memset(isps, 0, sizeof(isps));
	char *fmd5 = NULL;
	do_check_ip(p->fc, isps, &fmd5);
	while (1)
	{
		char *t = strchr(s, ',');
		if (t == NULL)
			break;
		*t = 0x0;
		if (check_self_in_shm(s, isps, p->f, p->d) == 0)
			continue;
		if (check_dup_task(s, p->d, p->f) == 0)
			continue;
		memset(srcip, 0, sizeof(srcip));
		memset(subbuf, 0, sizeof(subbuf));
		if (get_srcip(s, isps, srcip, p->f))
			l = snprintf(subbuf, sizeof(subbuf), "iplist=%s&vfs_cmd=M_SYNCFILE&%s=%s\n", s, p->d, p->f);
		else
			l = snprintf(subbuf, sizeof(subbuf), "iplist=%s&vfs_cmd=M_SYNCFILE&%s=%s,%s,%s\n", s, p->d, p->f, srcip, fmd5);
		if (add_dup_task(s, p->d, p->f))
		{
			LOG(vfs_http_log, LOG_ERROR, "add_dup_task err %s:%s:%s\n", s, p->d, p->f);
			return;
		}
		if (l + *ol >= SENDBUFSIZE)
		{
			LOG(vfs_http_log, LOG_ERROR, "hotfile %s too long ip %d:%d:%d\n", p->fc, l, *ol, SENDBUFSIZE);
			return;
		}
		*ol += snprintf(o + *ol, SENDBUFSIZE - *ol, "%s", subbuf);
		s = t + 1;
	}
}

static void do_syncfile(int type, t_sync_file *p, char *o, int *ol)
{
	switch(type)
	{
		case ALL:
			do_syncfile(ALLHOT, p, o, ol);
			do_syncfile(YD, p, o, ol);
			do_syncfile(HS, p, o, ol);
			do_syncfile(EDU, p, o, ol);
			do_syncfile(TT, p, o, ol);
			break;

		case ALLHOT:
			do_syncfile(TEL, p, o, ol);
			do_syncfile(CNC, p, o, ol);
			break;

		case CNC:
		case TEL:
		case YD:
		case HS:
		case EDU:
		case TT:
		case MP4:
		default:
			do_hot_sync(type, p, o, ol);
			break;

	}
}

static void put_sync_info_2_voss(char *buf, int l)
{
	LOG(vfs_http_log, LOG_NORMAL, "%s:%d\n", FUNC, LN);
	char fname[256] = {0x0};
	char stime[16] = {0x0};
	get_strtime(stime);

	snprintf(fname, sizeof(fname), "%s/voss_sync_%s.%d", voss_tmpdir, stime, g_index++);
	int fd = open(fname, O_CREAT | O_RDWR | O_LARGEFILE, 0644);
	if (fd < 0)
	{
		LOG(vfs_http_log, LOG_ERROR, "open %s err %m\n", fname);
		return ;
	}

	int wlen = write(fd, buf, l);
	if (l != wlen)
	{
		LOG(vfs_http_log, LOG_ERROR, "open %s err %m\n", fname);
		close(fd);
		return ;
	}
	close(fd);
	char fname2[256] = {0x0};
	snprintf(fname2, sizeof(fname2), "%s/voss_sync_%s.%d", voss_indir, stime, g_index);

	if (rename(fname, fname2))
	{
		LOG(vfs_http_log, LOG_ERROR, "rename %s to %s err %m\n", fname, fname2);
		return ;
	}
}

int hotfile_p(char *url, char *buf, int len)
{
	char *t = url;
	int l = 0; 
	char *p = strstr(t, SYNCFILE_SEP);
	if (p == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "err format hotfile %s\n", t);
		l += snprintf(buf+l, len -l , "err format hotfile\n");
		return l;
	}
	*p = 0x0;
	char *p1 = p + SYNCFILE_SEP_LEN;
	p = strchr(t, '/');
	if (p == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "err format hotfile %s\n", t);
		l += snprintf(buf+l, len -l , "err format hotfile\n");
		return l;
	}
	*p = 0x0;
	t_sync_file pp;
	char domain[64] = {0x0};
	snprintf(domain, sizeof(domain), "%s", t);
	if (strncmp(domain, "fcs", 3) || !strstr(domain, ".56.com"))
	{
		LOG(vfs_http_log, LOG_ERROR, "err format hotfile %s\n", t);
		l += snprintf(buf+l, len -l , "err format hotfile\n");
		return l;
	}
	pp.d = domain;
	p++;
	char fname[256] = {0x0};
	snprintf(fname, sizeof(fname), "%s/%s", hotfile_prefix, p);
	pp.f = fname;
	char fcname[256] = {0x0};
	snprintf(fcname, sizeof(fcname), "%s:%s", pp.d, fname);
	pp.fc = fcname;
	if (strcmp("all", p1) == 0)
		do_syncfile(ALL, &pp, buf, &l);
	else if (strcmp("allhot", p1) == 0)
		do_syncfile(ALLHOT, &pp, buf, &l);
	else if (strcmp("alltelhot", p1) == 0)
		do_syncfile(TEL, &pp, buf, &l);
	else if (strcmp("allcnchot", p1) == 0)
		do_syncfile(CNC, &pp, buf, &l);
	else if (strcmp("yd", p1) == 0)
		do_syncfile(YD, &pp, buf, &l);
	else if (strcmp("hs", p1) == 0)
		do_syncfile(HS, &pp, buf, &l);
	else if (strcmp("edu", p1) == 0)
		do_syncfile(EDU, &pp, buf, &l);
	else if (strcmp("tt", p1) == 0)
		do_syncfile(TT, &pp, buf, &l);
	else if (strcmp("mp4", p1) == 0)
		do_syncfile(MP4, &pp, buf, &l);
	else if (isalpha(*p1))
		do_syncfile(get_isp_by_name(p1), &pp, buf, &l);
	else
		do_syncfile_iplist(p1, &pp, buf, &l);
	if (l)
	{
		put_sync_info_2_voss(buf, l);
		l = snprintf(buf, len, "OK!\n");
	}
	else
		l = snprintf(buf, len, "OK!\n");
	return l;
}

static int check_rtime_shm(char *domain, char *f)
{
	char t[256] = {0x0};
	snprintf(t, sizeof(t), "%s:%s", domain, f);
	t_cdc_data *d;
	if (find_cdc_node(t, &d))
		return 0;
	t_cdc_val *v = &(d->v);
	time_t ntime = time(NULL) - MONTH_SECOND;
	if (ntime <= vfs_max(vfs_max(v->fmtime, v->frtime), v->fctime))
	{
		LOG(vfs_http_log, LOG_ERROR, "file %s rtime check!\n", t);
		return -1;
	}
	return 0;
}

static void del_from_shm_ip(char *domain, char *f, uint32_t ip)
{
	char t[256] = {0x0};
	snprintf(t, sizeof(t), "%s:%s", domain, f);
	t_cdc_data *d;
	if (find_cdc_node(t, &d))
		return ;
	t_cdc_val *v = &(d->v);
	int i = 0;
	for (i = 0; i < MAX_IP_IN_DIR; i++)
	{
		if (v->ip[i] != ip)
			continue;
		int s;
		get_n_s(i, &s, &(v->status_bists));
		set_n_s(i, CDC_F_DEL, &(v->status_bists));
		v->mtime[i] = time(NULL);
		LOG(vfs_http_log, LOG_NORMAL, "delete %u %s %s ok in shm\n", ip, domain, f);
	}
}

static void del_from_shm(t_cs_dir_info *cs, char *domain, char *f, uint32_t *ips, int *idx, uint8_t isp)
{
	uint32_t *ip = ips + *idx;
	char t[256] = {0x0};
	snprintf(t, sizeof(t), "%s:%s", domain, f);
	t_cdc_data *d;
	if (find_cdc_node(t, &d))
	{
		LOG(vfs_http_log, LOG_ERROR, "%s shm no record\n", t);
		return ;
	}
	t_cdc_val *v = &(d->v);
	int i = 0;
	for (i = 0; i < MAX_IP_IN_DIR; i++)
	{
		if (v->ip[i] == 0)
			continue;
		int s;
		get_n_s(i, &s, &(v->status_bists));
		if (s != CDC_F_OK)
			continue;
		if (isp == MAXISP)
		{
			if (check_ip_isp(v->ip[i], cs))
				continue;
		}
		else if (isp != ALL)
		{
			t_ip_info *ipinfo = NULL;
			if (get_ip_info_by_uint(&ipinfo, v->ip[i], 0, " ", " ") == 0)
			{
				if (ipinfo->isp != isp)
					continue;
			}
		}
		set_n_s(i, CDC_F_DEL, &(v->status_bists));
		v->mtime[i] = time(NULL);
		*ip = v->ip[i];
		LOG(vfs_http_log, LOG_NORMAL, "delete %u %s %s ok in shm\n", *ip, domain, f);
		ip++;
		(*idx)++;
	}
}

static void del_from_redis_ip(char *d, char *f, uint32_t ip)
{
    uint32_t tips[1024] = {0x0};
    get_redis(d, f, tips, sizeof(tips), c);
    int i = 0;
    uint32_t *tip = tips;
    for(; i < 1024 && *tip; tip++, i++){
        if (*tip == 0)
			return;
        if (*tip != ip)
			continue;
		if (del_ip_redis(d, f, ip, c))
			LOG(vfs_http_log, LOG_ERROR, "delete %u %s %s err\n", *tip, d, f);
		else
			LOG(vfs_http_log, LOG_NORMAL, "delete %u %s %s ok in redis\n", *tip, d, f);
		return;
	}
}

static void del_from_redis(t_cs_dir_info *cs, char *d, char *f, uint32_t *ips, int *idx, uint8_t isp)
{
	LOG(vfs_http_log, LOG_NORMAL, "delete %s %s prepare %d\n", d, f, *idx);
	uint32_t *ip = ips + *idx;
    uint32_t tips[1024] = {0x0};
    get_redis(d, f, tips, sizeof(tips), c);
    int i = 0;
    uint32_t *tip = tips;
    for(; i < 1024 && *tip; tip++, i++){
        if (*tip == 0)
			return;
		if (isp == MAXISP && check_ip_isp(*tip, cs) != 0)
			continue;
		else if (isp != ALL)
		{
			t_ip_info *ipinfo = NULL;
			if (get_ip_info_by_uint(&ipinfo, *tip, 0, " ", " ") == 0)
			{
				if (ipinfo->isp != isp)
					continue;
			}
		}
		if (del_ip_redis(d, f, *tip, c))
			LOG(vfs_http_log, LOG_ERROR, "delete %u %s %s err\n", *tip, d, f);
		else
			LOG(vfs_http_log, LOG_NORMAL, "delete %u %s %s ok in redis\n", *tip, d, f);
		*ip = *tip;
		ip++;
		(*idx)++;
	}
}

static int do_del_isp_file(uint8_t force, uint8_t isp, char *d, char *f, char *buf, int len, t_cs_dir_info *cs)
{
	LOG(vfs_http_log, LOG_NORMAL, "%s %s %s\n", FUNC, d, f);
	uint32_t ips[1024] = {0x0};
	if (isp == ALLHOT)
		get_hot_ips_uint(ips);
	else
		get_isp_ips_uint(isp, ips, 0);

	int l = 0;
	int i = 0;
	uint32_t *ip = ips;
	for (; *ip > 0 && i < 1024; i++, ip++)
	{
		if (force == 0 && check_ip_isp(*ip, cs))
			continue;
		del_from_redis_ip(d, f, *ip);
		del_from_shm_ip(d, f, *ip);

		char sip[16] = {0x0};
		ip2str(sip, *ip);
		LOG(vfs_http_log, LOG_NORMAL, "%s %s %s be delete\n", sip, d, f);
		l += snprintf(buf+l, len -l , "iplist=%s&vfs_cmd=M_DELFILE&%s=%s\n", sip, d, f);
	}
	put_sync_info_2_voss(buf, l);
	l = snprintf(buf, len, "OK!\n");
	return l;
}

int delfile_p(char *url, char *buf, int len)
{
	char *t = url;
	int l = 0; 
	uint8_t force = 0;
	uint8_t isp = MAXISP;
	char *p = NULL;
	if ((p = strstr(t, "&deltype=")) != NULL)
	{
		char *tt = strstr(p, "&force");
		if (tt)
		{
			*tt = 0x0;
			force = 1;
		}
		isp = get_isp_by_name(p+9);
		if (isp == UNKNOW_ISP)
		{
			LOG(vfs_http_log, LOG_ERROR, "err format delfile %s\n", t);
			l += snprintf(buf+l, len -l , "err format delfile\n");
			return l;
		}
		*p = '\0';
	}

	p = strchr(t, '/');
	if ( p == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "err format delfile %s\n", t);
		l += snprintf(buf+l, len -l , "err format delfile\n");
		return l;
	}
	*p = 0x0;
	char domain[64] = {0x0};
	snprintf(domain, sizeof(domain), "%s", t);
	if (strncmp(domain, "fcs", 3) || !strstr(domain, ".56.com"))
	{
		LOG(vfs_http_log, LOG_ERROR, "err format delfile %s\n", t);
		l += snprintf(buf+l, len -l , "err format delfile\n");
		return l;
	}
	p++;
	LOG(vfs_http_log, LOG_NORMAL, "delete type %d %s\n", isp, p);
	char fname[256] = {0x0};
	snprintf(fname, sizeof(fname), "%s/%s", hotfile_prefix, p);
	if (check_rtime_shm(domain, fname))
	{
		l += snprintf(buf+l, len -l , "file %s update in this month\n", fname);
		return l;
	}

	t_cs_dir_info cs;
	memset(&cs, 0, sizeof(cs));
	if (get_cs_info_by_path(fname, &cs))
	{
		LOG(vfs_http_log, LOG_ERROR, "get_cs_info_by_path err %s %m\n", fname);
		l += snprintf(buf+l, len -l , "err format delfile\n");
		return l;
	}
	if (isp <= ALLHOT && isp >= EDU)
		return do_del_isp_file(force, isp, domain, fname, buf, len, &cs);

	uint32_t delip[1024] = {0x0};
	int i = 0;
	int idx = 0;
	if (isp == ALL)
	{
		while (i < cs.index)
		{
			delip[i] = cs.ip[i];
			idx++;
			i++;
		}
	}
	del_from_shm(&cs, domain, fname, delip, &idx, isp);
	del_from_redis(&cs, domain, fname, delip, &idx, isp);

	uint32_t *ip = delip;
	i = 0;
	while (i < 1024)
	{
		if (*ip == 0)
			break;
		char sip[16] = {0x0};
		ip2str(sip, *ip);
		LOG(vfs_http_log, LOG_NORMAL, "%s %s %s be delete\n", sip, domain, fname);
		l += snprintf(buf+l, len -l , "iplist=%s&vfs_cmd=M_DELFILE&%s=%s\n", sip, domain, fname);
		i++;
		ip++;
	}

	put_sync_info_2_voss(buf, l);
	l = snprintf(buf, len, "OK!\n");
	return l;
}

static int get_archive_fcs(uint8_t archive_fcs, char *buf, int len)
{
	int domain = 0;
	int l = 0;
	while (1)
	{
		domain = get_next_fcs(domain, archive_fcs);
		if (domain == -1)
			break;
		l += snprintf(buf+l, len-l , "fcs%d.56.com;", domain);
	}
	return l;
}

int cfgquery_p(char *url, char *buf, int len)
{
	char *t = url;
	int l = 0;
	uint8_t isp = MAXISP;
	uint8_t archive_isp = MAXISP;
	uint8_t archive_fcs = MAXISP;
	char *f = NULL;
	if (strncmp(t, "isp=", 4) == 0)
	{
		f = strchr(t+4, ':');
		if (f)
		{
			*f = 0x0;
			f++;
		}
		isp = get_isp_by_name(t+4);
		if (isp == UNKNOW_ISP)
		{
			LOG(vfs_http_log, LOG_ERROR, "err format cfgquery_p %s\n", t);
			l += snprintf(buf+l, len -l , "err format cfgquery_p\n");
			return l;
		}
	}
	else if (strncmp(t, "archive_isp=", 12) == 0)
	{
		f = strchr(t+12, ':');
		if (f)
		{
			*f = 0x0;
			f++;
		}
		archive_isp = get_isp_by_name(t+12);
		if (archive_isp == UNKNOW_ISP)
		{
			LOG(vfs_http_log, LOG_ERROR, "err format cfgquery_p %s\n", t);
			l += snprintf(buf+l, len -l , "err format cfgquery_p\n");
			return l;
		}
	}
	else if (strncmp(t, "archive_fcs=", 12) == 0)
	{
		archive_fcs = get_isp_by_name(t+12);
		if (archive_fcs == UNKNOW_ISP)
		{
			LOG(vfs_http_log, LOG_ERROR, "err format cfgquery_p %s\n", t);
			l += snprintf(buf+l, len -l , "err format cfgquery_p\n");
			return l;
		}
	}
	else
	{
		LOG(vfs_http_log, LOG_ERROR, "err format cfgquery_p %s\n", t);
		l += snprintf(buf+l, len -l , "err format cfgquery_p\n");
		return l;
	}
	uint32_t ips[1024] = {0x0};
	if (isp != MAXISP)
	{
		if (isp == ALLHOT)
			get_hot_ips_uint(ips);
		else
			get_isp_ips_uint(isp, ips, 0);
	}
	else if (archive_isp != MAXISP)
		get_isp_ips_uint(archive_isp, ips, 1);
	else
		return get_archive_fcs(archive_fcs, buf, len);

	int i = 0;
	uint32_t *ip = ips;
	for (; *ip > 0 && i < 1024; i++, ip++)
	{
		char sip[16] = {0x0};
		ip2str(sip, *ip);
		l += snprintf(buf+l, len -l , "%s;", sip);
	}
	buf[l] = '\n';
	l++;
	return l;
}

static int do_syncdir(t_ip_info *ipinfo, uint32_t *fcss, int fcslen)
{
	if (ipinfo == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "%s:%d ipinfo is null\n", FUNC, LN);
		return -1;
	}

	int i = 0, j = 0, l = 0;
	char subbuf[1024] = {0x0};
	char tmp[256] = {0x0};
	l = snprintf(subbuf, sizeof(subbuf), "iplist=%s&vfs_cmd=M_SYNCDIR", ipinfo->s_ip);
	for (i = 0; i < MAXDIR_FOR_CS; i++)	 
	{
		if (strlen(ipinfo->dirs[i]) == 0)
				break;
		
		for (j = 0; j < fcslen; j++)
		{
			snprintf(tmp, sizeof(tmp), "&%s%u.%s=%s|19700101000000", g_config.domain_prefix, fcss[j], g_config.domain_suffix, ipinfo->dirs[i]);
			if (sizeof(subbuf) - l > strlen(tmp))
			{
				strcat(subbuf, tmp);
				l += strlen(tmp);
			}
			else
			{
				put_sync_info_2_voss(subbuf, l);
				l = snprintf(subbuf, sizeof(subbuf), "iplist=%s&vfs_cmd=M_SYNCDIR", ipinfo->sip);
				strcat(subbuf, tmp);
				l += strlen(tmp);
			}
		}
	}
	if (l > 0 && strchr(subbuf, '|') != NULL)
		put_sync_info_2_voss(subbuf, l);

	return 0;
}

int archive_p(char *url, char *buf, int len)
{
	char *query = url;
	char *p = NULL;
	char *t = NULL;
	char *cs_ips = NULL;
	int l = 0;
	int archive_isp = 0;

	//get the archive isp num
	if ((p = strstr(query, "group=")) == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "err format archive request, must have group param %s\n", query);
		l += snprintf(buf+l, len -l , "err format archive request, must have group param\n");
		return l;
	}
	
	p += 6;
	if ((t = strchr(p, '&')) != NULL)
		*t = '\0';

	archive_isp = get_isp_by_name(p);
	if (archive_isp == UNKNOW_ISP)
	{
		LOG(vfs_http_log, LOG_ERROR, "cant not find isp %s\n", p);
		l += snprintf(buf+l, len -l , "cant not find isp %s\n", p);
		return l;
	}
	LOG(vfs_http_log, LOG_DEBUG, "%s:%d isp:%s get isp num:%d\n", FUNC, LN, p, archive_isp);

	//get archive cs ip
	if (t != NULL)
		*t = '&';
	if ((cs_ips = strstr(query, "ip=")) != NULL)
	{
		cs_ips += 3;
		if ((t = strchr(cs_ips, '&')) != NULL)
			*t = '\0';
	}

	//get archive fcs array
	uint32_t fcs[MAXFCS] = {0x0};
	int domain = 0;
	int idx = 0;
	memset(fcs, 0, sizeof(fcs));
	while (1)
	{
		domain = get_next_fcs(domain, archive_isp);
		if (domain == -1)
			break;
		fcs[idx++] = domain;
	}
	LOG(vfs_http_log, LOG_DEBUG, "%s:%d get isp:%s fcs num:%d\n", FUNC, LN, p, idx);

	//get archive cs array and do create sync dir cmd
	if (get_cfg_lock())
	{
		LOG(vfs_http_log, LOG_ERROR, "archive_p get lock err, try later!\n");
		l += snprintf(buf+l, len -l , "archive_p get lock err, try later!\n");
		return l; 
	}
	list_head_t *hashlist = &(isp_iplist[archive_isp&MAXISP]);
	t_ip_info_list *server = NULL;
	list_head_t *ll = NULL;
	list_for_each_entry_safe_l(server, ll, hashlist, archive_list)
	{
		if (server->ipinfo.role == ROLE_CS)
		{
			if (cs_ips == NULL || (cs_ips != NULL && strstr(cs_ips, server->ipinfo.s_ip) != NULL))
			{
				LOG(vfs_http_log, LOG_DEBUG, "%s:%d get archive cs:%s\n", FUNC, LN, server->ipinfo.s_ip);
				do_syncdir(&(server->ipinfo), fcs, idx);
			}
		}
	}	
	release_cfg_lock();

	l = snprintf(buf, len, "OK!\n");
	return l;
}
