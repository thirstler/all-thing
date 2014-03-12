/*
 * at_agent.h
 *
 *  Created on: Feb 10, 2014
 *      Author: jrussler
 */

#ifndef AT_AGENT_H_
#define AT_AGENT_H_

/* Configured values */
#define IS_LINUX 1
#define KERNEL_VER_3

/* Default configuration values */
#define DEFAULT_CONFIG "/etc/allthing.conf"
#define DEFAULT_AG_POLL_RATE 5000000
#define DEFAULT_CPU_MULT 1
#define DEFAULT_IFACE_MULT 1
#define DEFAULT_IODEV_MULT 1
#define DEFAULT_FS_MULT 20
#define DEFAULT_RUNUSER "root"

#define DEFAULT_MASTER_MON_RATE 5000000
#define AGENT_PROFILE_DIR "/var/db/allthing"
#define AGENT_HOSTID_FILE "host_id"
#define RANDOM_DEVICE "/dev/random" /* not for crypto, so fine */
#define RANDOM_NEEDED 255

/* obscure defaults */
#define MAX_HOSTNAME_SIZE 255
#define LINE_BUFFER_SZ 1024
#define STR_BUFFER_SZ 255
#define PROC_CPU_BUFFER 128000
#define PROC_STAT_BUFFER 8192
#define PROC_DISKSTATS_BUFFER 8192
#define PROC_NETDEVSTATS_BUFFER 8192
#define PROC_MEMINF_BUFFER 2048
#define UDP_SEND_SZ 512
#define MASTER_ASMBL_BUF_AGE 15
#define ASMBL_BUF_PRUNE_COUNT 500

#define POLL_CPU         0x00000001
#define POLL_MEM         0x00000002
#define POLL_IODEV       0x00000004
#define POLL_IFACE       0x00000008
#define POLL_LOAD        0x00000010
#define POLL_UPTIME      0x00000020
#define POLL_FS          0x00000040
#define RPRT_CPU_SSTATIC 0x00000080

#define ASMBL_BUFFER_COMPLETE   0x00000001
#define ASMBL_BUFFER_INCOMPLETE 0x00000002

/* Typed definitions */
#include <sys/types.h>
#include <stdint.h>
#include <pthread.h>

/* Macros */

/* Unlink a node from a linked list, assumes the local presence of "first"
 * and "prev"
 */
#define LIST_UNLINK(node) {\
    if(first == node && node->next == NULL) {\
        first = NULL;\
        free(node);\
    } else if(first == node) {\
        first = node->next;\
        free(node);\
    } else {\
        prev->next = node->next;\
        free(node);\
    }\
}
typedef struct cpu_inf_calc_s {
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
} cpu_inf_calc_t;

typedef struct cpu_inf_s {
    u_int cpu;
    char *vendor_id;
    char *model;
    char *flags;
    char cache_units[4];
    u_int cache;
    u_int phy_id;
    u_int siblings;
    u_int core_id;
    u_int cpu_cores;
    float bogomips;
    float clock;
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
    void *next;

    cpu_inf_calc_t *calc;

} cpu_inf_t;

typedef struct iface_inf_calc_s {
    uint64_t recv_bytes;
    uint64_t recv_packets;
    uint64_t recv_errs;
    uint64_t recv_drop;
    uint64_t recv_fifo;
    uint64_t recv_frame;
    uint64_t recv_compressed;
    uint64_t recv_multicasts;
    uint64_t trns_bytes;
    uint64_t trns_packets;
    uint64_t trns_errs;
    uint64_t trns_drop;
    uint64_t trns_fifo;
    uint64_t trns_colls;
    uint64_t trns_carrier;
    uint64_t trns_compressed;
} iface_inf_calc_t;

typedef struct iface_inf_s {
    char *dev;
    char macaddr[18];
    char ipv4_addr[16];
    uint64_t recv_bytes;
    uint64_t recv_packets;
    uint64_t recv_errs;
    uint64_t recv_drop;
    uint64_t recv_fifo;
    uint64_t recv_frame;
    uint64_t recv_compressed;
    uint64_t recv_multicasts;
    uint64_t trns_bytes;
    uint64_t trns_packets;
    uint64_t trns_errs;
    uint64_t trns_drop;
    uint64_t trns_fifo;
    uint64_t trns_colls;
    uint64_t trns_carrier;
    uint64_t trns_compressed;
    void *next;

    iface_inf_calc_t *calc;

} iface_inf_t;


typedef struct iodev_inf_calc_s {
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
} iodev_inf_calc_t;

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
    void *next;

    iodev_inf_calc_t *calc;

} iodev_inf_t;

typedef struct fsinf_s {
    char *mountpoint;
    char *device;
    char *fstype;
    uint64_t block_size;
    uint64_t blks_total;
    uint64_t blks_free;
    uint64_t blks_avail;
    uint64_t inodes_ttl;
    uint64_t inodes_free;
    void *next;
} fsinf_t;

typedef struct sysinf_calc_s {

    /* total CPU rates */
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

} sysinf_calc_t;

typedef struct sysinf_s {
    char *hostname;
    unsigned int id;
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
    size_t cpu_count;        /* unused in agent */

    iface_inf_t *iface;
    size_t iface_count;        /* unused in agent */

    iodev_inf_t *iodev;
    size_t iodev_count;        /* unused in agent */

    fsinf_t *fsinf;
    size_t fsinf_count;        /* unused in agent */

    /* Calculated */
    sysinf_calc_t *calc;    /* unused in agent */

    void *next;                /* unused in agent */
    void *prev;                /* unused in agent */

} sysinf_t;


typedef struct agent_config_s {
    useconds_t master_rate; /* in microseconds */
    char *report_host;
    char *report_port;
    char *group;
    char *location;
    char *contact;
    char *runuser;
    char *config_file;
    char daemon;
    u_int cpu_mult;
    u_int cpu_sstatic_mult;
    u_int mem_mult;
    u_int iface_mult;
    u_int iodev_mult;
    u_int fs_mult;
    u_int sysload_mult;
    u_int uptime_mult;
} agent_config_t;


typedef struct master_config_s {
    useconds_t mon_rate;
    char *listen_port;
    char *config_file;
    char *runuser;
    char daemon;
    int rprt_hndlrs;

} master_config_t;

typedef struct json_str_assembly_bfr_s {
    uint64_t id;
    struct timeval ts;
    size_t charcount;
    uint64_t msgid;
    char *json_str;
    size_t strlen;
    void *next;
} json_str_assembly_bfr_t;

typedef struct rprt_hdr_s {
    uint64_t hostid;
    uint64_t seq;
    uint64_t msg_sz;
    uint64_t offset;
    uint64_t msgid;
    struct sockaddr fromaddr;
} rprt_hdr_t;


typedef struct master_global_data_s {
    pthread_mutex_t dispatcher; // The only unique data
    pthread_mutex_t *system_data_mutex;
    sysinf_t **system_data;
} master_global_data_t;

/* Global statistics */
typedef struct sysstats_s {
    u_int cpu_count;
    u_int iodev_count;
    u_int iface_count;
} sysstats_t;


#endif /* AT_AGENT_H_ */

/******************************************************************************
 * CPU operations ************************************************************/

void dump_cpudata(sysinf_t *host_data);

/* Run a CPU data poll */
inline void poll_cpus(sysinf_t *host_data);

/* Initialize the CPUs data structure. Removes any existing CPUs in the linked
 * list starts it over. */
cpu_inf_t* init_cpu(cpu_inf_t* cpu);

cpu_inf_t* push_cpu(cpu_inf_t* cpu, u_int proc);

cpu_inf_t* del_cpu(cpu_inf_t* cpu, u_int proc);

/* helper functions */
void init_proc(cpu_inf_t* cpu);

inline char* get_cpuinfstr(char* buffer);

/******************************************************************************
 * IFace operations **********************************************************/

void dump_ifacedata(iface_inf_t* iface);

void poll_iface(iface_inf_t* iface);

iface_inf_t* init_iface(iface_inf_t* devname);

iface_inf_t* del_iface(iface_inf_t* iface, char* devname);

iface_inf_t* push_iface(iface_inf_t* iface, char* devname);

void set_semistatic_iface_inf(iface_inf_t* iface);

/******************************************************************************
 * IODev operations **********************************************************/

void dump_iodevdata(iodev_inf_t* iodev);

iodev_inf_t* init_iodev();

iodev_inf_t* del_iodev(iodev_inf_t* iodev, char* devname);

iodev_inf_t* push_iodev(iodev_inf_t* dev, char *devname);

inline void poll_iodev(iodev_inf_t* iodev);

/******************************************************************************
 * File system operations ****************************************************/

inline void poll_fs(fsinf_t* fs);

fsinf_t* del_fs(fsinf_t* fs, char* mountpoint);

fsinf_t* init_fs(fsinf_t* fs);

/******************************************************************************
 * Master node listener ******************************************************/

void *report_listener(void *dptr);

/******************************************************************************
 * Master data ops ***********************************************************/

void get_data_buffer();

