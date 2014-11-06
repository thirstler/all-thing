/*
 * at_agent.h
 *
 *  Created on: Feb 10, 2014
 *      Author: jrussler
 */

#ifndef AT_H_
#define AT_H_

/* Default configuration values */
#define DEFAULT_CONFIG "/etc/allthing.conf"
#define DEFAULT_RUNUSER "root"
#define DEFAULT_REPORT_PORT "3456"
#define DEFAULT_LOG_LEVEL 4

/* send messages in tasty bits about this size*/
#define UDP_PAYLOAD_SZ 4096

typedef struct cpu_inf_s {
    u_int cpu;
    char *vendor_id;
    char *model;
    char *flags;
    char *cache_units;
    u_int cache;
    u_int phy_id;
    u_int siblings;
    u_int core_id;
    u_int cpu_cores;
    float bogomips;
    float cpuMhz;
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;
    uint64_t guest;
    uint64_t guest_nice;
    void *prev;
    void *next;

} cpu_inf_t;

typedef struct iface_inf_s {
    char *dev;
    char macaddr[18];
    char ipv4_addr[16];
    uint64_t rx_bytes;
    uint64_t rx_packets;
    uint64_t rx_errs;
    uint64_t rx_drop;
    uint64_t rx_fifo;
    uint64_t rx_frame;
    uint64_t rx_comp;
    uint64_t rx_multi;
    uint64_t tx_bytes;
    uint64_t tx_packets;
    uint64_t tx_errs;
    uint64_t tx_drop;
    uint64_t tx_fifo;
    uint64_t tx_colls;
    uint64_t tx_carr;
    uint64_t tx_comp;
    void *prev;
    void *next;

} iface_inf_t;

typedef struct iodev_inf_s {
    char *dev;
    uint64_t reads;
    uint64_t reads_merged;
    uint64_t read_sectors;
    uint64_t msec_reading;
    uint64_t writes;
    uint64_t writes_merged;
    uint64_t write_sectors;
    uint64_t msec_writing;
    uint64_t current_ios;
    uint64_t msec_ios;
    uint64_t weighted_ios;
    void *prev;
    void *next;
} iodev_inf_t;

typedef struct fsinf_s {
    char *mountpoint;
    char *dev;
    char *fstype;
    uint64_t block_size;
    uint64_t blks_total;
    uint64_t blks_free;
    uint64_t blks_avail;
    uint64_t inodes_ttl;
    uint64_t inodes_free;
    void *prev;
    void *next;
} fsinf_t;

typedef struct memval_s {
	uint64_t val;
	char unit[4];
} memval_t;

typedef struct ext_meminf_s {
	memval_t MemTotal;
	memval_t MemFree;
	memval_t MemAvailable;
	memval_t Buffers;
	memval_t Cached;
	memval_t SwapCached;
	memval_t Active;
	memval_t Inactive;
	memval_t Active_anon;
	memval_t Inactive_anon;
	memval_t Active_file;
	memval_t Inactive_anon;
	memval_t Unevictable;
	memval_t Mlocked;
	memval_t SwapTotal;
	memval_t SwapFree;
	memval_t Dirty;
	memval_t Writeback;
	memval_t AnonPages;
	memval_t Mapped;
	memval_t Shmem;
	memval_t Slab;
	memval_t SReclaimable;
	memval_t SUnreclaim;
	memval_t KernelStack;
	memval_t PageTables;
	memval_t NFS_Unstable;
	memval_t Bounce;
	memval_t WritebackTmp;
	memval_t CommitLimit;
	memval_t Committed_AS;
	memval_t VmallocTotal;
	memval_t VmallocUsed;
	memval_t VmallocChunk;
	memval_t HardwareCorrupted;
	memval_t AnonHugePages;
	memval_t HugePages_Total;
	memval_t HugePages_Free;
	memval_t HugePages_Rsvd;
	memval_t HugePages_Surp;
	memval_t Hugepagesize;
	memval_t DirectMap4k;
	memval_t DirectMap2M;
	memval_t DirectMap1G;
} ext_meminf_t;

typedef struct sysinf_s {
    char *hostname;
    uint64_t id;
    struct timeval sample_tv;

    /* Load and process information */
    float uptime;
    float idletime;
    float load_1;
    float load_5;
    float load_15;
    u_int procs_running;
    u_int procs_total;

    /* Memory information */
    uint64_t mem_total;
    uint64_t mem_free;
    uint64_t mem_buffers;
    uint64_t mem_cache;
    uint64_t swap_free;
    uint64_t swap_total;

    /* total CPU values */
    uint64_t interrupts;
    uint64_t s_interrupts;
    uint64_t context_switches;
    uint64_t cpu_user;
    uint64_t cpu_nice;
    uint64_t cpu_system;
    uint64_t cpu_idle;
    uint64_t cpu_iowait;
    uint64_t cpu_irq;
    uint64_t cpu_sirq;
    uint64_t cpu_steal;
    uint64_t cpu_guest;
    uint64_t cpu_guest_nice;

    /* Dynamic device info */
    cpu_inf_t *cpu;
    iface_inf_t *iface;
    iodev_inf_t *iodev;
    fsinf_t *fsinf;

} sysinf_t;

#endif /* AT_H_ */
