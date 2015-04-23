#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <pwd.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifdef USE_CJSON
#include "../cJSON.h"
#else
#include <jansson.h>
#endif
#include "../ini.h"
#include "../at.h"
#include "at_agent.h"

int on;
int out_sock;
int init_flag;
uint64_t hostid;
char hexid[17];
sysinf_t sysinf;

/* Debugging functions *******************************************************/
# ifdef DEBUG
static void dump_cfg(agent_config_t *cfg)
{
    printf("Config:\n");
    printf("  Report host     : %s\n", cfg->report_host);
    printf("  Report port     : %d\n", cfg->report_port);
    printf("  Master poll rate: %d\n", cfg->master_rate);
    printf("  CPU mult        : %d\n", cfg->cpu_mult);
    printf("  FS mult         : %d\n", cfg->fs_mult);
    printf("  iface mult      : %d\n", cfg->iface_mult);
    printf("  I/O dev mult    : %d\n", cfg->iodev_mult);
    printf("  mem mult        : %d\n", cfg->mem_mult);
}

static void dump_load(sysinf_t *host_data)
{
    printf("Load stats:\n");
    printf("  1 min         : %f\n", host_data->load_1);
    printf("  5 min         : %f\n", host_data->load_5);
    printf("  15 min        : %f\n", host_data->load_15);
    printf("  procs running : %d\n", host_data->procs_running);
    printf("  procs total   : %d\n", host_data->procs_total);
}

static void dump_mem(sysinf_t *host_data)
{
    printf("Memory:\nMemTotal: %lu\n", host_data->mem_total);
    printf("MemFree: %lu\n", host_data->mem_free);
    printf("Buffers: %lu\n", host_data->mem_buffers);
    printf("Cached: %lu\n", host_data->mem_cache);
    printf("SwapTotal: %lu\n", host_data->swap_total);
    printf("SwapFree: %lu\n", host_data->swap_free);
}
#endif

/******************************************************************************
 * Initializers
 *****************************************************************************/

/*
 * Set defaults values defined in at.h
 */
static void set_cfg_defaults(agent_config_t* cfg)
{
    cfg->master_rate = DEFAULT_AG_POLL_RATE;
    cfg->cpu_mult = DEFAULT_CPU_MULT;
    cfg->iface_mult = DEFAULT_IFACE_MULT;
    cfg->iodev_mult = DEFAULT_IODEV_MULT;
    cfg->fs_mult = DEFAULT_FS_MULT;
    cfg->metadata_mult = DEFAULT_METADATA_MULT;
    cfg->runuser = strdup(DEFAULT_RUNUSER);
    cfg->config_file = strdup(DEFAULT_CONFIG);
    cfg->location = NULL;
    cfg->contact = NULL;
    cfg->group = NULL;
    cfg->daemon = 1;
    cfg->log_level = DEFAULT_LOG_LEVEL;
    cfg->msg_chunk_sz = UDP_PAYLOAD_SZ;
}

static void set_data_hostname(sysinf_t* hd)
{
    char hnbuf[MAX_HOSTNAME_SIZE];
    int rc = gethostname(hnbuf, MAX_HOSTNAME_SIZE);
    if( rc == 0 ) {
        hd->hostname = strdup(hnbuf);
    } else {
        strcpy(hd->hostname, "localhost.localdomain");
    }
}

static void init_hostdata(sysinf_t *host_data)
{

    if(host_data->hostname != NULL) free(host_data->hostname);
    if(host_data->cpu != NULL) free(host_data->cpu);
    if(host_data->iface != NULL) free(host_data->iface);
    if(host_data->iodev != NULL) free(host_data->iodev);
    if(host_data->fsinf != NULL) free(host_data->fsinf);

    host_data->cpu = init_cpu(host_data->cpu);
    host_data->iface = init_iface(host_data->iface);
    host_data->iodev = init_iodev(host_data->iodev);
    host_data->fsinf = init_fs(host_data->fsinf);

    /* Needed from the start */
    set_data_hostname(host_data);

    hostid = 0;
}

static inline const char* get_mem_val(const char *key, memval_t* val, const char *strptr)
{
	const char *ring = strptr;
	ring = strstr(ring, key);
	int rc;

	rc = sscanf(ring, "%*s: %lu %s", &val->val, val->unit);
	if(rc != 2 || rc == EOF) return strptr;
	return ring;
}

static inline void poll_mem(sysinf_t *host_data)
{
    FILE *fh = fopen("/proc/meminfo", "r");
    char meminfbuf[PROC_MEMINF_BUFFER];
    char *fp;

    fread(meminfbuf, PROC_MEMINF_BUFFER, 1, fh);
    fclose(fh);

    sscanf(fp = strstr(meminfbuf, "MemTotal:"), "MemTotal: %lu", &host_data->mem.MemTotal);
    sscanf(fp = strstr(meminfbuf, "MemFree:"), "MemFree: %lu", &host_data->mem.MemFree);
    sscanf(fp = strstr(meminfbuf, "MemAvailable:"), "MemAvailable: %lu", &host_data->mem.MemAvailable);
    sscanf(fp = strstr(meminfbuf, "Buffers:"), "Buffers: %lu", &host_data->mem.Buffers);
    sscanf(fp = strstr(meminfbuf, "Cached:"), "Cached: %lu", &host_data->mem.Cached);
    sscanf(fp = strstr(meminfbuf, "SwapCached:"), "SwapCached: %lu", &host_data->mem.SwapCached);
    sscanf(fp = strstr(meminfbuf, "Active:"), "Active: %lu", &host_data->mem.Active);
    sscanf(fp = strstr(meminfbuf, "Inactive:"), "Inactive: %lu", &host_data->mem.Inactive);
    sscanf(fp = strstr(meminfbuf, "Active(anon):"), "Active(anon): %lu", &host_data->mem.Active_anon_);
    sscanf(fp = strstr(meminfbuf, "Inactive(anon):"), "Inactive(anon): %lu", &host_data->mem.Inactive_anon_);
    sscanf(fp = strstr(meminfbuf, "Active(file):"), "Active(file): %lu", &host_data->mem.Active_file_);
    sscanf(fp = strstr(meminfbuf, "Inactive(file):"), "Inactive(file): %lu", &host_data->mem.Inactive_file_);
    sscanf(fp = strstr(meminfbuf, "Unevictable:"), "Unevictable: %lu", &host_data->mem.Unevictable);
    sscanf(fp = strstr(meminfbuf, "Mlocked:"), "Mlocked: %lu", &host_data->mem.Mlocked);
    sscanf(fp = strstr(meminfbuf, "SwapTotal:"), "SwapTotal: %lu", &host_data->mem.SwapTotal);
    sscanf(fp = strstr(meminfbuf, "SwapFree:"), "SwapFree: %lu", &host_data->mem.SwapFree);
    sscanf(fp = strstr(meminfbuf, "Dirty:"), "Dirty: %lu", &host_data->mem.Dirty);
    sscanf(fp = strstr(meminfbuf, "Writeback:"), "Writeback: %lu", &host_data->mem.Writeback);
    sscanf(fp = strstr(meminfbuf, "AnonPages:"), "AnonPages: %lu", &host_data->mem.AnonPages);
    sscanf(fp = strstr(meminfbuf, "Mapped:"), "Mapped: %lu", &host_data->mem.Mapped);
    sscanf(fp = strstr(meminfbuf, "Shmem:"), "Shmem: %lu", &host_data->mem.Shmem);
    sscanf(fp = strstr(meminfbuf, "Shmem:"), "Shmem: %lu", &host_data->mem.Shmem);
    sscanf(fp = strstr(meminfbuf, "Slab:"), "Slab: %lu", &host_data->mem.Slab);
    sscanf(fp = strstr(meminfbuf, "SReclaimable:"), "SReclaimable: %lu", &host_data->mem.SReclaimable);
    sscanf(fp = strstr(meminfbuf, "SUnreclaim:"), "SUnreclaim: %lu", &host_data->mem.SUnreclaim);
    sscanf(fp = strstr(meminfbuf, "KernelStack:"), "KernelStack: %lu", &host_data->mem.KernelStack);
    sscanf(fp = strstr(meminfbuf, "PageTables:"), "PageTables: %lu", &host_data->mem.PageTables);
    sscanf(fp = strstr(meminfbuf, "NFS_Unstable:"), "NFS_Unstable: %lu", &host_data->mem.NFS_Unstable);
    sscanf(fp = strstr(meminfbuf, "Bounce:"), "Bounce: %lu", &host_data->mem.Bounce);
    sscanf(fp = strstr(meminfbuf, "WritebackTmp:"), "WritebackTmp: %lu", &host_data->mem.WritebackTmp);
    sscanf(fp = strstr(meminfbuf, "CommitLimit:"), "CommitLimit: %lu", &host_data->mem.CommitLimit);
    sscanf(fp = strstr(meminfbuf, "Committed_AS:"), "Committed_AS: %lu", &host_data->mem.Committed_AS);
    sscanf(fp = strstr(meminfbuf, "VmallocTotal:"), "VmallocTotal: %lu", &host_data->mem.VmallocTotal);
    sscanf(fp = strstr(meminfbuf, "VmallocUsed:"), "VmallocUsed: %lu", &host_data->mem.VmallocUsed);
    sscanf(fp = strstr(meminfbuf, "VmallocChunk:"), "VmallocChunk: %lu", &host_data->mem.VmallocChunk);
    sscanf(fp = strstr(meminfbuf, "HardwareCorrupted:"), "HardwareCorrupted: %lu", &host_data->mem.HardwareCorrupted);
    sscanf(fp = strstr(meminfbuf, "AnonHugePages:"), "AnonHugePages: %lu", &host_data->mem.AnonHugePages);
    sscanf(fp = strstr(meminfbuf, "HugePages_Total:"), "HugePages_Total: %lu", &host_data->mem.HugePages_Total);
    sscanf(fp = strstr(meminfbuf, "HugePages_Free:"), "HugePages_Free: %lu", &host_data->mem.HugePages_Free);
    sscanf(fp = strstr(meminfbuf, "HugePages_Rsvd:"), "HugePages_Rsvd: %lu", &host_data->mem.HugePages_Rsvd);
    sscanf(fp = strstr(meminfbuf, "HugePages_Surp:"), "HugePages_Surp: %lu", &host_data->mem.HugePages_Surp);
    sscanf(fp = strstr(meminfbuf, "Hugepagesize:"), "Hugepagesize: %lu", &host_data->mem.Hugepagesize);
    sscanf(fp = strstr(meminfbuf, "DirectMap4k:"), "DirectMap4k: %lu", &host_data->mem.DirectMap4k);
    sscanf(fp = strstr(meminfbuf, "DirectMap2M:"), "DirectMap2M: %lu", &host_data->mem.DirectMap2M);
    sscanf(fp = strstr(meminfbuf, "DirectMap1G:"), "DirectMap1G: %lu", &host_data->mem.DirectMap1G);
}

static inline void poll_uptime(sysinf_t *host_data)
{
    FILE *fh = fopen("/proc/uptime", "r");
    fscanf(fh, "%f %f", &host_data->uptime, &host_data->idletime);
    fclose(fh);
}

static inline void poll_load(sysinf_t *host_data)
{
    FILE *fh = fopen("/proc/loadavg", "r");
    fscanf(fh, "%f %f %f %u/%u",
            &host_data->load_1,
            &host_data->load_5,
            &host_data->load_15,
            &host_data->procs_running,
            &host_data->procs_total);
    fclose(fh);
}

static int config_handler(
        void* user,
        const char* section,
        const char* name,
        const char* value)
{
    agent_config_t* cfg = (agent_config_t*)user;

    if(strcmp(section, "global") == 0) {
        if(strcmp(name, "master_hostname") == 0)
            cfg->report_host = strdup(value);
        if(strcmp(name, "master_target_port") == 0)
            cfg->report_port = strdup(value);
    }

    if(strcmp(section, "agent") == 0) {
        if(strcmp(name, "master_poll_rate") == 0)
            cfg->master_rate = atol(value);
        if(strcmp(name, "cpu.poll_mult") == 0)
            cfg->cpu_mult = atoi(value);
        if(strcmp(name, "cpus_static.poll_mult") == 0)
            cfg->cpus_static_mult = atoi(value);
        if(strcmp(name, "metadata.poll_mult") == 0)
            cfg->metadata_mult = atoi(value);
        if(strcmp(name, "memory.poll_mult") == 0)
            cfg->mem_mult = atoi(value);
        if(strcmp(name, "diskio.poll_mult") == 0)
            cfg->iodev_mult = atoi(value);
        if(strcmp(name, "netio.poll_mult") == 0)
            cfg->iface_mult = atoi(value);
        if(strcmp(name, "fs.poll_mult") == 0)
            cfg->fs_mult = atoi(value);
        if(strcmp(name, "sysload.poll_mult") == 0)
            cfg->sysload_mult = atoi(value);
        if(strcmp(name, "uptime.poll_mult") == 0)
            cfg->uptime_mult = atoi(value);
        if(strcmp(name, "log_level") == 0)
            cfg->log_level = atoi(value);
        if(strcmp(name, "msg_send_size") == 0)
            cfg->msg_chunk_sz = atoi(value);
        if(strcmp(name, "location") == 0) {
            if(cfg->location != NULL) free(cfg->location);
            cfg->location = strdup(value);
        }
        if(strcmp(name, "contact") == 0) {
            if(cfg->contact != NULL) free(cfg->contact);
            cfg->contact = strdup(value);
        }
        if(strcmp(name, "group") == 0) {
            if(cfg->group != NULL) free(cfg->group);
            cfg->group = strdup(value);
        }
        if(strcmp(name, "runuser") == 0) {
            if(cfg->runuser != NULL) free(cfg->runuser);
            cfg->runuser = strdup(value);
        }
    }
    return 1;
}

inline int get_poll_bits(int count, agent_config_t *cfg)
{
    int sw = 0;
    if( (count % cfg->cpu_mult) == 0 ) sw |= POLL_CPU;
    if( (count % cfg->iodev_mult) == 0 ) sw |= POLL_IODEV;
    if( (count % cfg->iface_mult) == 0 ) sw |= POLL_IFACE;
    if( (count % cfg->mem_mult) == 0)sw |= POLL_MEM;
    if( (count % cfg->sysload_mult) == 0) sw |= POLL_LOAD;
    if( (count % cfg->uptime_mult) == 0) sw |= POLL_UPTIME;
    if( (count % cfg->fs_mult) == 0) sw |= POLL_FS;
    if( (count % cfg->cpus_static_mult) == 0) sw |= RPRT_CPU_SSTATIC;
    if( (count % cfg->metadata_mult) == 0) sw |= RPRT_METADATA;
    return sw;
}

#ifdef USE_CJSON
static inline char* jsonify(sysinf_t *host_data, agent_config_t *cfg, int pollsw)
{
    fsinf_t *fsptr;
    iface_inf_t *ifaceptr;
    iodev_inf_t *iodevptr;
    cpu_inf_t *cpuptr;
    char *endgame;
    cJSON *devptr;
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "hostid", hexid);
    
    cJSON *misc = cJSON_CreateObject();
    cJSON_AddStringToObject(misc, "hostname", host_data->hostname);
    if(pollsw & POLL_UPTIME) {
        cJSON_AddNumberToObject(misc, "uptime", host_data->uptime);
        cJSON_AddNumberToObject(misc, "idletime", host_data->idletime);
        cJSON_AddItemToObject(root, "misc", misc);
    }
    
    cJSON *ts = cJSON_CreateObject();
    cJSON_AddNumberToObject(ts, "tv_sec", host_data->sample_tv.tv_sec);
    cJSON_AddNumberToObject(ts, "tv_usec", host_data->sample_tv.tv_usec);
    cJSON_AddItemToObject(root, "ts", ts);
    

    if(pollsw & RPRT_METADATA) {
    	cJSON *meta = cJSON_CreateObject();
        cJSON_AddStringToObject(meta, "location", cfg->location);
        cJSON_AddStringToObject(meta, "contact", cfg->contact);
        cJSON_AddStringToObject(meta, "group", cfg->group);
        cJSON_AddItemToObject(root, "meta", meta);
    }
    
    if(pollsw & POLL_FS) {
        
        cJSON *fsdevs = cJSON_CreateArray();
        
        fsptr = host_data->fsinf;
        while(fsptr != NULL) {

        	/* Mow this over every iteration */
        	devptr =  cJSON_CreateObject();
            cJSON_AddStringToObject(devptr, "dev", fsptr->dev);
            cJSON_AddStringToObject(devptr, "mountpoint", fsptr->mountpoint);
            cJSON_AddStringToObject(devptr, "fstype", fsptr->fstype);
            cJSON_AddNumberToObject(devptr, "fstype", fsptr->blks_total);
            cJSON_AddNumberToObject(devptr, "blks_free", fsptr->blks_free);
            cJSON_AddNumberToObject(devptr, "blks_avail", fsptr->blks_avail);
            cJSON_AddNumberToObject(devptr, "block_size", fsptr->block_size);
            cJSON_AddNumberToObject(devptr, "inodes_ttl", fsptr->inodes_ttl);
            cJSON_AddNumberToObject(devptr, "inodes_free", fsptr->inodes_free);
            cJSON_AddItemToArray(fsdevs, devptr);
            fsptr = fsptr->next;

        }
        cJSON_AddItemToObject(root, "fs", fsdevs);
    }

    if(pollsw & POLL_IFACE) {

    	cJSON *ifdevs = cJSON_CreateArray();

    	ifaceptr = host_data->iface;
    	while(ifaceptr != NULL) {
    		/* We don't need inactive devices */
			if(ifaceptr->rx_packets+ifaceptr->tx_packets == 0) {
				ifaceptr = ifaceptr->next;
				continue;
			}

			/* Mow this over every iteration */
			devptr =  cJSON_CreateObject();

			cJSON_AddStringToObject(devptr, "dev", ifaceptr->dev);
			cJSON_AddNumberToObject(devptr, "c|rx_packets", ifaceptr->rx_packets);
			cJSON_AddNumberToObject(devptr, "c|rx_bytes", ifaceptr->rx_bytes);
			cJSON_AddNumberToObject(devptr, "c|rx_errs", ifaceptr->rx_errs);
			cJSON_AddNumberToObject(devptr, "c|rx_drop", ifaceptr->rx_drop);
			cJSON_AddNumberToObject(devptr, "c|rx_fifo", ifaceptr->rx_fifo);
			cJSON_AddNumberToObject(devptr, "c|rx_frame", ifaceptr->rx_frame);
			cJSON_AddNumberToObject(devptr, "c|rx_comp", ifaceptr->rx_comp);
			cJSON_AddNumberToObject(devptr, "c|rx_multi", ifaceptr->rx_multi);
			cJSON_AddNumberToObject(devptr, "c|tx_packets", ifaceptr->tx_packets);
			cJSON_AddNumberToObject(devptr, "c|tx_bytes", ifaceptr->tx_bytes);
			cJSON_AddNumberToObject(devptr, "c|tx_errs", ifaceptr->tx_errs);
			cJSON_AddNumberToObject(devptr, "c|tx_drop", ifaceptr->tx_drop);
			cJSON_AddNumberToObject(devptr, "c|c|tx_fifo", ifaceptr->tx_fifo);
			cJSON_AddNumberToObject(devptr, "c|tx_colls", ifaceptr->tx_colls);
			cJSON_AddNumberToObject(devptr, "c|tx_carr", ifaceptr->tx_carr);
			cJSON_AddNumberToObject(devptr, "c|tx_comp", ifaceptr->tx_comp);
			cJSON_AddItemToArray(ifdevs, devptr);

			ifaceptr = ifaceptr->next;
    	}
    	cJSON_AddItemToObject(root, "iface", ifdevs);
    }

    if(pollsw & POLL_IODEV) {

    	cJSON *iodevs = cJSON_CreateArray();

    	iodevptr = host_data->iodev;
    	while(iodevptr != NULL) {
    		/* We don't need inactive devices */
			if(iodevptr->reads+iodevptr->writes == 0) {
				iodevptr = iodevptr->next;
				continue;
			}

			/* Mow this over every iteration */
			devptr =  cJSON_CreateObject();

			cJSON_AddStringToObject(devptr, "dev", iodevptr->dev);
			cJSON_AddNumberToObject(devptr, "c|reads", iodevptr->reads);
			cJSON_AddNumberToObject(devptr, "c|read_sectors", iodevptr->read_sectors);
			cJSON_AddNumberToObject(devptr, "c|reads_merged", iodevptr->reads_merged);
			cJSON_AddNumberToObject(devptr, "c|msec_reading", iodevptr->msec_reading);
			cJSON_AddNumberToObject(devptr, "c|writes", iodevptr->writes);
			cJSON_AddNumberToObject(devptr, "c|write_sectors", iodevptr->write_sectors);
			cJSON_AddNumberToObject(devptr, "c|writes_merged", iodevptr->writes_merged);
			cJSON_AddNumberToObject(devptr, "c|msec_writing", iodevptr->msec_writing);
			cJSON_AddNumberToObject(devptr, "current_ios", iodevptr->current_ios);
			cJSON_AddNumberToObject(devptr, "c|msec_ios", iodevptr->msec_ios);
			cJSON_AddNumberToObject(devptr, "c|weighted_ios", iodevptr->weighted_ios);
			cJSON_AddItemToArray(iodevs, devptr);

			iodevptr = iodevptr->next;
    	}
    	cJSON_AddItemToObject(root, "iodev", iodevs);
    }

    if(pollsw & POLL_LOAD) {
    	cJSON *load = cJSON_CreateObject();
    	cJSON_AddNumberToObject(load, "1min", host_data->load_1);
    	cJSON_AddNumberToObject(load, "5min", host_data->load_5);
    	cJSON_AddNumberToObject(load, "15min", host_data->load_15);
    	cJSON_AddNumberToObject(load, "procs_run", host_data->procs_running);
    	cJSON_AddNumberToObject(load, "procs_total", host_data->procs_total);
    	cJSON_AddItemToObject(root, "sysload", load);
    }

    if(pollsw & POLL_CPU) {

    	cJSON *cputtls = cJSON_CreateObject();
    	cJSON_AddNumberToObject(cputtls, "c|user", host_data->cpu_user);
    	cJSON_AddNumberToObject(cputtls, "c|nice", host_data->cpu_nice);
    	cJSON_AddNumberToObject(cputtls, "c|system", host_data->cpu_system);
    	cJSON_AddNumberToObject(cputtls, "c|idle", host_data->cpu_idle);
    	cJSON_AddNumberToObject(cputtls, "c|iowait", host_data->cpu_iowait);
    	cJSON_AddNumberToObject(cputtls, "c|irq", host_data->cpu_irq);
    	cJSON_AddNumberToObject(cputtls, "c|s_irq", host_data->cpu_sirq);
    	cJSON_AddNumberToObject(cputtls, "c|steal", host_data->cpu_steal);
    	cJSON_AddNumberToObject(cputtls, "c|guest", host_data->cpu_guest);
    	cJSON_AddNumberToObject(cputtls, "c|n_guest", host_data->cpu_guest_nice);
    	cJSON_AddNumberToObject(cputtls, "c|intrps", host_data->interrupts);
    	cJSON_AddNumberToObject(cputtls, "c|s_intrps", host_data->s_interrupts);
    	cJSON_AddNumberToObject(cputtls, "c|ctxt", host_data->context_switches);
    	cJSON_AddItemToObject(root, "cpu_ttls", cputtls);

    	cJSON *cpus = cJSON_CreateArray();
    	cpuptr = host_data->cpu;
		while(cpuptr != NULL) {

			/* Mow this over every iteration */
			devptr =  cJSON_CreateObject();

			cJSON_AddNumberToObject(devptr, "cpu", cpuptr->cpu);
			cJSON_AddNumberToObject(devptr, "c|user", cpuptr->user);
			cJSON_AddNumberToObject(devptr, "c|nice", cpuptr->nice);
			cJSON_AddNumberToObject(devptr, "c|system", cpuptr->system);
			cJSON_AddNumberToObject(devptr, "c|idle", cpuptr->idle);
			cJSON_AddNumberToObject(devptr, "c|iowait", cpuptr->iowait);
			cJSON_AddNumberToObject(devptr, "c|irq", cpuptr->irq);
			cJSON_AddNumberToObject(devptr, "c|s_irq", cpuptr->softirq);
			cJSON_AddNumberToObject(devptr, "c|steal", cpuptr->steal);
			cJSON_AddNumberToObject(devptr, "c|guest", cpuptr->guest);
			cJSON_AddNumberToObject(devptr, "c|n_guest", cpuptr->guest_nice);
			cJSON_AddNumberToObject(devptr, "clock", cpuptr->cpuMhz);
			cJSON_AddItemToArray(cpus, devptr);

			cpuptr = cpuptr->next;
		}
		cJSON_AddItemToObject(root, "cpus", cpus);
    }

    if(pollsw & RPRT_CPU_SSTATIC) {

    	cJSON *cpus_static = cJSON_CreateArray();

    	cpuptr = host_data->cpu;
    	while(cpuptr != NULL) {

			/* Mow this over every iteration */
			devptr =  cJSON_CreateObject();

			cJSON_AddNumberToObject(devptr, "cpu", cpuptr->cpu);
			cJSON_AddStringToObject(devptr, "vendor_id", cpuptr->vendor_id);
			cJSON_AddStringToObject(devptr, "model", cpuptr->model);
			cJSON_AddStringToObject(devptr, "flags", cpuptr->flags);
			cJSON_AddNumberToObject(devptr, "cache", cpuptr->cache);
			cJSON_AddStringToObject(devptr, "cache_units", cpuptr->cache_units);
			cJSON_AddNumberToObject(devptr, "phy_id", cpuptr->phy_id);
			cJSON_AddNumberToObject(devptr, "siblings", cpuptr->siblings);
			cJSON_AddNumberToObject(devptr, "core_id", cpuptr->core_id);
			cJSON_AddNumberToObject(devptr, "cpu_cores", cpuptr->cpu_cores);
			cJSON_AddNumberToObject(devptr, "bogomips", cpuptr->bogomips);
			cJSON_AddItemToArray(cpus_static, devptr);

			cpuptr = cpuptr->next;
    	}
    	cJSON_AddItemToObject(root, "cpus_static", cpus_static);
    }

    if(pollsw & POLL_MEM) {
    	cJSON *mem = cJSON_CreateObject();
    	cJSON_AddNumberToObject(mem, "total", host_data->mem_total);
    	cJSON_AddNumberToObject(mem, "free", host_data->mem_free);
    	cJSON_AddNumberToObject(mem, "buffers", host_data->mem_buffers);
    	cJSON_AddNumberToObject(mem, "cache", host_data->mem_cache);
    	cJSON_AddNumberToObject(mem, "swap_free", host_data->swap_free);
    	cJSON_AddNumberToObject(mem, "swap_total", host_data->swap_total);
    	cJSON_AddItemToObject(root, "memory", mem);
    }

    endgame = cJSON_Print(root);
    cJSON_Delete(root);
    cJSON_Minify(endgame);
    return endgame;
}
#else
static inline char* jsonify(sysinf_t *host_data, agent_config_t *cfg, int pollsw)
{

    json_t *root, *tmpar, *tmpobj;
    cpu_inf_t *cpuptr;
    iodev_inf_t *iodevptr;
    iface_inf_t *ifaceptr;
    fsinf_t *fsptr;
    char *res = NULL;

    root = json_object();

    tmpobj = json_pack("{ss}", "hostname", host_data->hostname);

    if(pollsw & POLL_UPTIME) {
        json_object_set_new(tmpobj, "uptime", json_real(host_data->uptime));
        json_object_set_new(tmpobj, "idletime", json_real(host_data->idletime));
    }

    json_object_set_new(root, "misc", tmpobj);

    tmpobj = json_pack("{sIsI}",
            "tv_sec",host_data->sample_tv.tv_sec,
            "tv_usec", host_data->sample_tv.tv_usec);
    json_object_set_new(root, "ts", tmpobj);
    json_object_set_new(root, "hostid", json_string_nocheck(hexid));


    if(pollsw & RPRT_METADATA) {
    	tmpobj = json_pack("{ssssss}",
    			"location", cfg->location,
    			"contact", cfg->contact,
    			"group", cfg->group);
    	json_object_set_new(root, "meta", tmpobj);
    }

    if(pollsw & POLL_FS) {

        tmpar = json_array();
        fsptr = host_data->fsinf;
        while(fsptr != NULL) {

            tmpobj = json_pack(
                    "{sssssssIsIsIsIsIsI}",
                    "dev", fsptr->dev,
                    "mountpoint", fsptr->mountpoint,
                    "fstype", fsptr->fstype,
                    "blks_total", fsptr->blks_total,
                    "blks_free", fsptr->blks_free,
                    "blks_avail", fsptr->blks_avail,
                    "block_size", fsptr->block_size,
                    "inodes_ttl", fsptr->inodes_ttl,
                    "inodes_free", fsptr->inodes_free);

            json_array_append_new(tmpar, tmpobj);

            fsptr = fsptr->next;
        }
        json_object_set_new(root, "fs", tmpar);
    }

    if(pollsw & POLL_IFACE) {

        tmpar = json_array();
        ifaceptr = host_data->iface;
        while(ifaceptr != NULL) {

        	/* We don't need inactive devices */
        	if(ifaceptr->rx_packets+ifaceptr->tx_packets == 0) {
        		ifaceptr = ifaceptr->next;
        		continue;
        	}

            tmpobj = json_pack("{sssIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsI}",
                    "dev", ifaceptr->dev,
                    "c|rx_packets", ifaceptr->rx_packets,
                    "c|rx_bytes", ifaceptr->rx_bytes,
                    "c|rx_errs", ifaceptr->rx_errs,
                    "c|rx_drop", ifaceptr->rx_drop,
                    "c|rx_fifo", ifaceptr->rx_fifo,
                    "c|rx_frame", ifaceptr->rx_frame,
                    "c|rx_comp", ifaceptr->rx_comp,
                    "c|rx_multi", ifaceptr->rx_multi,
                    "c|tx_packets", ifaceptr->tx_packets,
                    "c|tx_bytes", ifaceptr->tx_bytes,
                    "c|tx_errs", ifaceptr->tx_errs,
                    "c|tx_drop", ifaceptr->tx_drop,
                    "c|tx_fifo", ifaceptr->tx_fifo,
                    "c|tx_colls", ifaceptr->tx_colls,
                    "c|tx_carr", ifaceptr->tx_carr,
                    "c|tx_comp", ifaceptr->tx_comp);

            json_array_append_new(tmpar, tmpobj);

            ifaceptr = ifaceptr->next;
        }
        json_object_set_new(root, "iface", tmpar);
    }

    if(pollsw & POLL_IODEV) {

        tmpar = json_array();
        iodevptr = host_data->iodev;
        while(iodevptr != NULL) {

        	/* We don't need inactive devices */
        	if(iodevptr->reads+iodevptr->writes == 0) {
        		iodevptr = iodevptr->next;
        		continue;
        	}

            tmpobj = json_pack("{sssIsIsIsIsIsIsIsIsIsIsI}",
                    "dev", iodevptr->dev,
                    "c|reads", iodevptr->reads,
                    "c|read_sectors", iodevptr->read_sectors,
                    "c|reads_merged", iodevptr->reads_merged,
                    "c|msec_reading", iodevptr->msec_reading,
                    "c|writes", iodevptr->writes,
                    "c|write_sectors", iodevptr->write_sectors,
                    "c|writes_merged", iodevptr->writes_merged,
                    "c|msec_writing", iodevptr->msec_writing,
                    "current_ios", iodevptr->current_ios,
                    "c|msec_ios", iodevptr->msec_ios,
                    "c|weighted_ios", iodevptr->weighted_ios);

            json_array_append_new(tmpar, tmpobj);

            iodevptr = iodevptr->next;
        }
        json_object_set_new(root, "iodev", tmpar);
    }

    if(pollsw & POLL_LOAD) {
        tmpobj = json_pack("{sfsfsfsIsI}",
                "1min", host_data->load_1,
                "5min", host_data->load_5,
                "15min", host_data->load_15,
                "procs_run", host_data->procs_running,
                "procs_total", host_data->procs_total);
        json_object_set_new(root, "sysload", tmpobj);
    }

    if(pollsw & POLL_CPU) {

        /* CPUs aggregate */
        tmpobj = json_pack("{sIsIsIsIsIsIsIsIsIsIsIsIsI}",
                "c|user", host_data->cpu_user,
                "c|nice", host_data->cpu_nice,
                "c|system", host_data->cpu_system,
                "c|idle", host_data->cpu_idle,
                "c|iowait", host_data->cpu_iowait,
                "c|irq", host_data->cpu_irq,
                "c|s_irq", host_data->cpu_sirq,
                "c|steal", host_data->cpu_steal,
                "c|guest", host_data->cpu_guest,
                "c|n_guest", host_data->cpu_guest_nice,
                "c|intrps", host_data->interrupts,
                "c|s_intrps", host_data->s_interrupts,
                "c|ctxt", host_data->context_switches);

        json_object_set_new(root, "cpu_ttls", tmpobj);

        /* individual CPUS */
        tmpar = json_array();
        cpuptr = host_data->cpu;
        while(cpuptr != NULL) {

            tmpobj = json_pack("{sIsIsIsIsIsIsIsIsIsIsIsf}",
                    "cpu", cpuptr->cpu,
                    "c|user", cpuptr->user,
                    "c|nice", cpuptr->nice,
                    "c|system", cpuptr->system,
                    "c|idle", cpuptr->idle,
                    "c|iowait", cpuptr->iowait,
                    "c|irq", cpuptr->irq,
                    "c|s_irq", cpuptr->softirq,
                    "c|steal", cpuptr->steal,
                    "c|guest", cpuptr->guest,
                    "c|n_guest", cpuptr->guest_nice,
                    "clock", cpuptr->cpuMhz);

            json_array_append_new(tmpar, tmpobj);

            cpuptr = cpuptr->next;
        }
        json_object_set_new(root, "cpus", tmpar);
    }

    if(pollsw & RPRT_CPU_SSTATIC) {

        /* individual CPUS */
        tmpar = json_array();
        cpuptr = host_data->cpu;
        while(cpuptr != NULL) {

            tmpobj = json_pack("{sIsssssssIsssIsIsIsIsf}",
                    "cpu", cpuptr->cpu,
                    "vendor_id", cpuptr->vendor_id,
                    "model", cpuptr->model,
                    "flags", cpuptr->flags,
                    "cache", cpuptr->cache,
                    "cache_units", cpuptr->cache_units,
                    "phy_id", cpuptr->phy_id,
                    "siblings", cpuptr->siblings,
                    "core_id", cpuptr->core_id,
                    "cpu_cores", cpuptr->cpu_cores,
                    "bogomips", cpuptr->bogomips);

            json_array_append_new(tmpar, tmpobj);

            cpuptr = cpuptr->next;
        }
        json_object_set_new(root, "cpus_static", tmpar);
    }

    if(pollsw & POLL_MEM) {

        tmpobj = json_pack("{sIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsIsI}",
                "MemTotal", host_data->mem.MemTotal,
                "MemFree", host_data->mem.MemFree,
                "MemAvailable", host_data->mem.MemAvailable,
                "Buffers", host_data->mem.Buffers,
                "Cached", host_data->mem.Cached,
                "SwapCached", host_data->mem.SwapCached,
                "Active", host_data->mem.Active,
                "Inactive", host_data->mem.Inactive,
                "Active(anon)", host_data->mem.Active_anon_,
                "Inactive(anon)", host_data->mem.Inactive_anon_,
                "Active(file)", host_data->mem.Active_file_,
                "Inactive(file)", host_data->mem.Inactive_file_,
                "Unevictable", host_data->mem.Unevictable,
                "Mlocked", host_data->mem.Mlocked,
                "SwapTotal", host_data->mem.SwapTotal,
                "SwapFree", host_data->mem.SwapFree,
                "Dirty", host_data->mem.Dirty,
                "Writeback", host_data->mem.Writeback,
                "AnonPages", host_data->mem.AnonPages,
                "Mapped", host_data->mem.Mapped,
                "Shmem", host_data->mem.Shmem,
                "Slab", host_data->mem.Slab,
                "SReclaimable", host_data->mem.SReclaimable,
                "SUnreclaim", host_data->mem.SUnreclaim,
                "KernelStack", host_data->mem.KernelStack,
                "PageTables", host_data->mem.PageTables,
                "NFS_Unstable", host_data->mem.NFS_Unstable,
                "Bounce", host_data->mem.Bounce,
                "WritebackTmp", host_data->mem.WritebackTmp,
                "CommitLimit", host_data->mem.CommitLimit,
                "Committed_AS", host_data->mem.Committed_AS,
                "VmallocTotal", host_data->mem.VmallocTotal,
                "VmallocUsed", host_data->mem.VmallocUsed,
                "VmallocChunk", host_data->mem.VmallocChunk,
                "HardwareCorrupted", host_data->mem.HardwareCorrupted,
                "AnonHugePages", host_data->mem.AnonHugePages,
                "HugePages_Total", host_data->mem.HugePages_Total,
                "HugePages_Free", host_data->mem.HugePages_Free,
                "HugePages_Rsvd", host_data->mem.HugePages_Rsvd,
                "HugePages_Surp", host_data->mem.HugePages_Surp,
                "Hugepagesize", host_data->mem.Hugepagesize,
                "DirectMap4k", host_data->mem.DirectMap4k,
                "DirectMap2M", host_data->mem.DirectMap2M,
                "DirectMap1G", host_data->mem.DirectMap1G);
                
        json_object_set_new(root, "memory", tmpobj);
    }

    res = json_dumps(root, JSON_COMPACT);
    //res = json_dumps(root, JSON_INDENT(2));

    json_object_clear(root);
    json_decref(root);

    return res;
}
#endif

static inline ssize_t report(char *json_str, agent_config_t *cfg)
{
    static uint64_t msgid = 0;
    size_t payload_len = strlen(json_str);
    size_t hdr_chunk = 80;
    size_t msg_chunk = cfg->msg_chunk_sz-hdr_chunk;

    size_t sent = 0;
    size_t seq = 0;
    size_t written = 0;
    char sendbuffer[cfg->msg_chunk_sz+1];
    char *bfrptr;
    char *msgptr = json_str;
    ssize_t bytes_sent = 0;

    for(sent=0; bytes_sent < payload_len; sent += msg_chunk) {

        memset(sendbuffer, '\0', cfg->msg_chunk_sz+1);
        bfrptr = sendbuffer;

        written = sprintf(bfrptr,
                "%lx %lx %lx %lx %lx\n",
                hostid, seq, payload_len, bytes_sent, msgid);
        bfrptr += written;

        memcpy(bfrptr, msgptr, msg_chunk);

        send(out_sock, sendbuffer, strlen(sendbuffer), 0);
        
        bytes_sent += msg_chunk;
        msgptr += msg_chunk; // May advance into oblivion. Who cares?
        seq += 1;

    }
    msgid += 1;

    return bytes_sent;
}

static int netsock(agent_config_t *cfg)
{
    struct addrinfo hints;
    struct addrinfo *res;
    int rsock = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    int rc = getaddrinfo(cfg->report_host, cfg->report_port, &hints, &res);
    if(rc != 0) return 0;

    rsock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    /* All UDP but I want rsock to be a standing descriptor */
    connect(rsock, res->ai_addr, res->ai_addrlen);

    free(res);

    return rsock;
}

static void quitit(int sig)
{
    syslog(LOG_INFO, "exit requested");
    on = 0;
}


static void reinit(int sig)
{
    printf("SIGHUP (%d) requested\n", sig);
    fflush(stdout);
    init_flag = 1;
}

static void cleanup(sysinf_t *host_data, agent_config_t *cfg)
{
    char sendbuffer[255];

    /* Report to master that this node has quit and close the socket */
    sprintf(sendbuffer, "<<%lx exited>>\n", hostid);
    send(out_sock, sendbuffer, strlen(sendbuffer), 0);
    close(out_sock);

    /* Clean up data structures */
    while(host_data->cpu != NULL) {
        host_data->cpu = del_cpu(host_data->cpu, host_data->cpu->cpu);
    }
    while(host_data->fsinf != NULL) {
        host_data->fsinf = del_fs(host_data->fsinf, host_data->fsinf->mountpoint);
    }
    while(host_data->iface != NULL) {
        host_data->iface = del_iface(host_data->iface, host_data->iface->dev);
    }
    while(host_data->iodev != NULL) {
        host_data->iodev = del_iodev(host_data->iodev, host_data->iodev->dev);
    }

    if(host_data->hostname != NULL) free(host_data->hostname);
    if(host_data != NULL) free(host_data);
    if(cfg->contact != NULL) free(cfg->contact);
    if(cfg->group != NULL) free(cfg->group);
    if(cfg->location != NULL) free(cfg->location);
    if(cfg->report_host != NULL) free(cfg->report_host);
    if(cfg->report_port != NULL) free(cfg->report_port);
    if(cfg->runuser != NULL) free(cfg->runuser);
    if(cfg->config_file != NULL) free(cfg->config_file);
    free(cfg);
}


static uint64_t mkrnd64() {
    uint64_t n;
    FILE *r = fopen("/dev/random", "r");
    if(r == NULL) return 0;
    printf("%lu\n", sizeof(uint64_t));
    fread(&n, sizeof(uint64_t), 1, r);
    fclose(r);
    return n;
}



/* This is a little off the wall since this used to generate a host ID based
 * off of a hash of the host name. Why? I have no idea - now it's just a
 * random number. */
static uint64_t profile_id(char *forced_id, agent_config_t *cfg)
{
    FILE *fh;
    char hostid_path[255];
    char hexid[17];
    uint64_t id;
    struct stat fsinfo;
    struct passwd *pw = NULL;

    /* Skip all this junk if we're forcing an id */
    if(forced_id != NULL)
        return mkrnd64();

    pw = getpwnam(cfg->runuser);

    sprintf(hostid_path, "%s/%s", AGENT_PROFILE_DIR, AGENT_HOSTID_FILE);
    fh = fopen(hostid_path, "r");

    if(fh == NULL) {


        /* Directory ? */
        if(stat(AGENT_PROFILE_DIR, &fsinfo) != 0) {
            syslog(LOG_WARNING,
                    "problem with db dir, attempting to create");
            if (mkdir(AGENT_PROFILE_DIR, S_IRUSR|S_IWUSR|S_IXUSR) != 0) {
                goto fail;
            }
        }

        chown(AGENT_PROFILE_DIR, pw->pw_uid, pw->pw_gid);

        /* The file */
        fh = fopen(hostid_path, "a");
        if(fh == NULL) {
            syslog(LOG_ERR, "can't write to host id file (%s)",
                    hostid_path);
            goto fail;
        }

        chown(hostid_path, pw->pw_uid, pw->pw_gid);
        chmod(hostid_path, S_IRUSR|S_IWUSR);

        syslog(LOG_INFO, "generating a random ID for this host...");

        id = mkrnd64();

        sprintf(hexid, "%lx", id);

        /* Write them to the new profile file */
        if(fwrite(hexid, sizeof(hexid), 1, fh) == 0) {
            syslog(LOG_ERR, "problem writing to host id file");
            goto fail;
        }
        fclose(fh);

        syslog(LOG_INFO, "finished generating random ID");

    } else {
        fscanf(fh, "%lx", &id);
        fclose(fh);
    }

    /* Do all of this again for good measure */
    chown(AGENT_PROFILE_DIR, pw->pw_uid, pw->pw_gid);
    chown(hostid_path, pw->pw_uid, pw->pw_gid);
    chmod(hostid_path, S_IRUSR|S_IWUSR);

    return id;

    fail:
        syslog(LOG_ERR, "can't get host ID, exiting");
        return 0;
}

int main(int argc, char *argv[])
{
    sysinf_t *host_data;
    agent_config_t *cfg;
    unsigned int pollcount = 0;
    int poll_sw = 0;
    int rc;
    char *json_str;
    struct passwd *mememe;
    char *forced_id = NULL;
    FILE *pidf;
    pid_t mypid;

    init_flag = 0;
    on = 1;
    memset(&sysinf, 0, sizeof(sysinf));

    openlog("at_agent", LOG_CONS|LOG_PERROR, LOG_USER);

    /* Allocate root host data object and set defaults */
    host_data = malloc(sizeof(sysinf_t));
    memset(host_data, '\0', sizeof(sysinf_t));
    init_hostdata(host_data);

    /* Allocate config object and set defaults */
    cfg = malloc(sizeof(agent_config_t));
    memset(cfg, 0, sizeof(agent_config_t));
    set_cfg_defaults(cfg);

    /* Now overwrite with config file */
    if (ini_parse(cfg->config_file, config_handler, cfg) < 0) {
        syslog(LOG_ERR, "Can't load '%s'\n", cfg->config_file);
    }

    /* See what the command line has to say.... */
    while ((rc = getopt (argc, argv, "hDt:p:r:c:i:")) != -1) {
        switch(rc) {
        case 'h':
        	printf("%s", AT_AGENT_HELP);
        	return EXIT_SUCCESS;
        case 'D':
            cfg->daemon = 0;
            break;
        case 'i':
            forced_id = strdup(optarg);
            break;
        case 't':
            cfg->report_host = strdup(optarg);
            break;
        case 'p':
            cfg->report_port = strdup(optarg);
            break;
        case 'r':
            cfg->master_rate = atoi(optarg)*1000000;
            break;
        case 'c':
            cfg->config_file = strdup(optarg);
            if (ini_parse(cfg->config_file, config_handler, cfg) < 0) {
                syslog(LOG_ERR, "Can't load '%s'", cfg->config_file);
            }
            break;
        case '?':
            syslog(LOG_ERR, "argument expected");
            break;
        default:
            fprintf(stderr, "unexpected flag: %c", rc);
        }
    }

    /* done gathering options, let's get down to businesss... */

    syslog (LOG_CRIT, "starting");

    /* Get this host ID */
    hostid = profile_id(forced_id, cfg);
    if(forced_id != NULL) free(forced_id);
    host_data->id = hostid;
    
    /* Set a string for the hostid */
    sprintf(hexid, "%lx", hostid);

    syslog(LOG_INFO, "using host id %lx", hostid);

    /* forking */
    if(cfg->daemon) {
        rc = fork();
        if(rc != 0) {
            exit(EXIT_SUCCESS);
        }
        fclose(stderr);
        fclose(stdout);
        openlog("at_agent", LOG_CONS, LOG_USER);
        syslog(LOG_DEBUG, "daemonizing");
    } else {
        openlog("at_agent", LOG_CONS|LOG_PERROR, LOG_USER);
        syslog(LOG_DEBUG, "running in foreground");
    }

    mypid = getpid();
    pidf = fopen(AGENT_PID_FILE, "w");
    fprintf(pidf, "%u", mypid);
    fclose(pidf);

    /* Set user */
    mememe = getpwnam(cfg->runuser);
    if(mememe != NULL) {
        setreuid(mememe->pw_uid, mememe->pw_uid);
        setregid(mememe->pw_gid, mememe->pw_gid);
    } else {
        syslog(LOG_ERR, "running as root! please configure a user.\n");
    }

    /* Set the log mask */
	rc = 0;
	switch(cfg->log_level) {
	case 7: rc |= LOG_MASK(LOG_DEBUG);
	case 6: rc |= LOG_MASK(LOG_INFO);
	case 5: rc |= LOG_MASK(LOG_NOTICE);
	case 4: rc |= LOG_MASK(LOG_WARNING);
	case 3: rc |= LOG_MASK(LOG_ERR);
	case 2: rc |= LOG_MASK(LOG_CRIT);
	case 1: rc |= LOG_MASK(LOG_ALERT);
	case 0: rc |= LOG_MASK(LOG_EMERG);
	}
	setlogmask(rc);

	printf("log level/mask %i/%x\n", cfg->log_level, rc);

    /* Handle some signals */
    if (signal (SIGINT, quitit) == SIG_IGN)
            signal (SIGINT, SIG_IGN);
    if (signal (SIGTERM, quitit) == SIG_IGN)
                signal (SIGTERM, SIG_IGN);
    if (signal (SIGHUP, reinit) == SIG_IGN)
             signal (SIGHUP, SIG_IGN);

    /* Establish connection to master server */
    out_sock = netsock(cfg);
    if(out_sock == 0) {
        syslog(LOG_CRIT, "failed to get network socket, I quit");
        return(EXIT_FAILURE);
    }

    for(pollcount=0 ;on; pollcount += 1) {

        /* Flip some bits to see what we're polling this time around */
        poll_sw = get_poll_bits(pollcount, cfg);

        /* Poll system based on multiplier schedule */
        if( poll_sw & POLL_CPU ) poll_cpus(host_data);
        if( poll_sw & POLL_IODEV ) poll_iodev(host_data->iodev);
        if( poll_sw & POLL_IFACE ) poll_iface(host_data->iface);
        if( poll_sw & POLL_MEM ) poll_mem(host_data);
        if( poll_sw & POLL_LOAD ) poll_load(host_data);
        if( poll_sw & POLL_UPTIME ) poll_uptime(host_data);
        if( poll_sw & POLL_FS ) poll_fs(host_data->fsinf);

        /* Time-stamp the sample */
        gettimeofday(&host_data->sample_tv, NULL);

        json_str = jsonify(host_data, cfg, poll_sw);

        report(json_str, cfg);
        free(json_str);

        usleep(cfg->master_rate);

        /* If SIGHUP is received, reread config, reinitialize host data
         * structure and reset network connection to master host (since it
         * might have changed. */
        if(init_flag) {
            syslog(LOG_INFO, "re-initializing host data");
            if (ini_parse(cfg->config_file, config_handler, cfg) < 0) {
                syslog(LOG_ERR, "Can't load '%s'\n", cfg->config_file);
            }
            init_hostdata(host_data);

            close(out_sock);
            out_sock = netsock(cfg);
            init_flag = 0;
        }
        pollcount += 1;
    }
    /* Out of main loop */

    cleanup(host_data, cfg);
    syslog(LOG_CRIT, "exited");
    closelog();

    return EXIT_SUCCESS;
}
