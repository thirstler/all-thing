#ifndef AT_AGENT_H_
#define AT_AGENT_H_

#define AGENT_PID_FILE "/var/run/at_agent.pid"
#define DEFAULT_METADATA_MULT 60
#define DEFAULT_CPU_MULT 1
#define DEFAULT_IFACE_MULT 1
#define DEFAULT_IODEV_MULT 1
#define DEFAULT_AG_POLL_RATE 5000000
#define DEFAULT_FS_MULT 20

/* poll flags */
#define POLL_CPU         0x00000001
#define POLL_MEM         0x00000002
#define POLL_IODEV       0x00000004
#define POLL_IFACE       0x00000008
#define POLL_LOAD        0x00000010
#define POLL_UPTIME      0x00000020
#define POLL_FS          0x00000040
#define RPRT_CPU_SSTATIC 0x00000080
#define RPRT_METADATA    0x00000100

#define AGENT_PROFILE_DIR "/var/db/allthing"
#define AGENT_HOSTID_FILE "host_id"

/* random defaults */
#define MAX_HOSTNAME_SIZE 255
#define PROC_MEMINF_BUFFER 2048
#define PROC_NETDEVSTATS_BUFFER 8192
#define PROC_DISKSTATS_BUFFER 8192
#define PROC_STAT_BUFFER 8192
#define PROC_CPU_BUFFER 128000
#define STR_BUFFER_SZ 255
#define LINE_BUFFER_SZ 1024


#define AT_AGENT_HELP "\n\
Use: at_agent [OPTION]\n\
Start the all-thing agent daemon.\n\
\n\
Arguments:\n\
  -h, --help            this help\n\
  -D, --nofork          do not fork into a daemon (usefule for debugging)\n\
  -p, --port=PORT       change the reporting port\n\
  -t, --target=HOST     change target (at_master) host for data reports\n\
  -i, --forceid=ID      use arbitrary string for node ID (text is hashed to\n\
                        generate node ID)\n\
  -r, --pollrate=MS     change system information gathering poll rate, rate\n\
                        is set in milliseconds\n\
  -c, --config=FILE     use a different config file\n\
  -d, --debug=LEVEL     specify a debug level (0-7)\n\
\n\
"

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

typedef struct agent_config_s {
    useconds_t master_rate;
    char *report_host;
    char *report_port;
    char *group;
    char *location;
    char *contact;
    char *runuser;
    char *config_file;
    char daemon;
    int log_level;
    u_int msg_chunk_sz;
    u_int cpu_mult;
    u_int cpus_static_mult;
    u_int mem_mult;
    u_int iface_mult;
    u_int iodev_mult;
    u_int fs_mult;
    u_int sysload_mult;
    u_int uptime_mult;
    u_int metadata_mult;
} agent_config_t;

/* Global statistics */
typedef struct agent_sysstats_s {
    u_int cpu_count;
    u_int iodev_count;
    u_int iface_count;
} agent_sysstats_t;


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
 * Iface operations **********************************************************/

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

#endif