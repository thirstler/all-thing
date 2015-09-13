/*
 * File: at.h
 * Desc: Shared header file for agent and master source trees. Defines some
 *       common data structures.
 *
 * copyright 2015 Jason Russler
 *
 * This file is part of AllThing.
 *
 * AllThing is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AllThing is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.*Z
 *
 * You should have received a copy of the GNU General Public License
 * along with AllThing.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AT_H_
#define AT_H_

#include <uuid/uuid.h>

/* Default configuration values */

/* Config dir WITH trailing slash :-) */
#define DEFAULT_CONFIG_DIR "/etc/allthing/"
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

typedef struct meminf_s {
    uint64_t MemTotal;
    uint64_t MemFree;
    uint64_t MemAvailable;
    uint64_t Buffers;
    uint64_t Cached;
    uint64_t SwapCached;
    uint64_t Active;
    uint64_t Inactive;
    uint64_t Active_anon_;
    uint64_t Inactive_anon_;
    uint64_t Active_file_;
    uint64_t Inactive_file_;
    uint64_t Unevictable;
    uint64_t Mlocked;
    uint64_t SwapTotal;
    uint64_t SwapFree;
    uint64_t Dirty;
    uint64_t Writeback;
    uint64_t AnonPages;
    uint64_t Mapped;
    uint64_t Shmem;
    uint64_t Slab;
    uint64_t SReclaimable;
    uint64_t SUnreclaim;
    uint64_t KernelStack;
    uint64_t PageTables;
    uint64_t NFS_Unstable;
    uint64_t Bounce;
    uint64_t WritebackTmp;
    uint64_t CommitLimit;
    uint64_t Committed_AS;
    uint64_t VmallocTotal;
    uint64_t VmallocUsed;
    uint64_t VmallocChunk;
    uint64_t HardwareCorrupted;
    uint64_t AnonHugePages;
    uint64_t HugePages_Total;
    uint64_t HugePages_Free;
    uint64_t HugePages_Rsvd;
    uint64_t HugePages_Surp;
    uint64_t Hugepagesize;
    uint64_t DirectMap4k;
    uint64_t DirectMap2M;
    uint64_t DirectMap1G;
} meminf_t;

typedef struct sysinf_s {
    char *hostname;
    uuid_t uuid;
    char uuidstr[37];
    struct timeval sample_tv;

    /* Load and process information */
    float uptime;
    float idletime;
    float load_1;
    float load_5;
    float load_15;
    u_int procs_running;
    u_int procs_total;

    meminf_t mem;

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
