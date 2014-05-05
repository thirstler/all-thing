/*
 * mstr_dataops.c
 *
 *  Created on: Mar 3, 2014
 *      Author: jrussler
 */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <syslog.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <jansson.h>
#include "at.h"

#ifdef DEBUG
static void dump_dataobj(sysinf_t *target)
{
	printf("Hostname:      %s\n", target->hostname);
	printf("Host ID:       %lx\n", target->id);
	printf("Uptime:        %f\n", target->uptime);
	printf("Idle:          %f\n", target->idletime);
	printf("Interrupts:    %lu\n", target->interrupts);
	printf("Context sw:    %lu\n", target->context_switches);
	printf("Sysload --\n");
	printf("    1 min:     %f\n", target->load_1);
	printf("    5 min:     %f\n", target->load_5);
	printf("   15 min:     %f\n", target->load_15);
	printf("Memory --\n");
	printf("    Total:     %lu\n", target->mem_total);
	printf("     Free:     %lu\n", target->mem_free);
	printf("    Cache:     %lu\n", target->mem_cache);
	printf("  Buffers:     %lu\n", target->mem_buffers);
	printf("  swTotal:     %lu\n", target->swap_total);
	printf("   swFree:     %lu\n", target->swap_free);
	printf("CPU ttls (counters) --\n");
	printf("     CPUs:     %lu\n", target->cpu_count);
	printf("     User:     %lu\n", target->cpu_user);
	printf("   System:     %lu\n", target->cpu_system);
	printf("     Nice:     %lu\n", target->cpu_nice);
	printf("     Idle:     %lu\n", target->cpu_idle);
	printf("   IOWait:     %lu\n", target->cpu_iowait);
	printf("      IRQ:     %lu\n", target->cpu_irq);
	printf("     sIRQ:     %lu\n", target->cpu_sirq);
	printf("    Guest:     %lu\n", target->cpu_guest);
	printf("   nGuest:     %lu\n", target->cpu_guest_nice);

	cpu_inf_t *cpu = target->cpu;
	while(cpu != NULL) {
		printf(" CPU %u --\n", cpu->cpu);
		printf("    Cores:     %u\n", cpu->cpu_cores);
		printf("   CoreID:     %u\n", cpu->core_id);
		printf(" Siblings:     %u\n", cpu->siblings);
		printf("   Phy ID:     %u\n", cpu->phy_id);
		printf("   Vendor:     %s\n", cpu->vendor_id);
		printf("    Model:     %s\n", cpu->model);
		printf("    Cache:     %u %s\n", cpu->cache, cpu->cache_units);
		printf(" Bogomips:     %f\n", cpu->bogomips);
		printf("    Clock:     %f\n", cpu->cpuMhz);
		printf("    Flags:     %s\n", cpu->flags);
		printf("     User:     %lu\n", cpu->user);
		printf("   System:     %lu\n", cpu->system);
		printf("     Nice:     %lu\n", cpu->nice);
		printf("     Idle:     %lu\n", cpu->idle);
		printf("   IOWait:     %lu\n", cpu->iowait);
		printf("      IRQ:     %lu\n", cpu->irq);
		printf("     sIRQ:     %lu\n", cpu->softirq);
		printf("    Guest:     %lu\n", cpu->guest);
		printf("   nGuest:     %lu\n", cpu->guest_nice);
		cpu = cpu->next;
	}
}
#endif

inline sysinf_t* new_data_obj(uint64_t newid)
{
    sysinf_t *newobj = malloc(sizeof(sysinf_t));
    memset(newobj, '\0', sizeof(sysinf_t));
    newobj->id = newid;
    return newobj;
}

inline int rm_data_obj(uint64_t id, master_global_data_t *data)
{
	size_t i;
	int shift=0;

	for (i = 0; i < data->system_data_sz; i += 1) {
		if( data->system_data[i]->id == id ) {
			free_data_obj(data->system_data[i]);
			shift = 1;
		}
		if(shift == 1) {
			data->system_data[i] = ((i+1) < data->system_data_sz) ? data->system_data[i+1] : NULL;
		}
	}
	return EXIT_SUCCESS;
}


inline sysinf_t* get_data_obj(uint64_t id, master_global_data_t *data)
{
	// Brute force for now, I'll get fancy later
	size_t i;

	for(i = 0; i < data->system_data_sz; i += 1) {
		if ( data->system_data[i]->id == id ) return data->system_data[i];
	}
	return NULL;
}

inline int add_new_data_obj(master_global_data_t *master, sysinf_t *newobj)
{
	size_t i;
	if( master->system_data_sz >= DATA_STRUCT_AR_SIZE) {
		syslog(LOG_ERR, "data buffer full, increase to > %d",
				DATA_STRUCT_AR_SIZE);
		return EXIT_FAILURE;
	}

	for(i=0; i < master->system_data_sz; i += 1) {
		if ( master->system_data[i]->id == newobj->id) return EXIT_FAILURE;
	}
	master->system_data[master->system_data_sz] = newobj;
	master->system_data_sz += 1;


	printf("added %lx, new size %lu\n", newobj->id, master->system_data_sz);
	return EXIT_SUCCESS;
}

int calc_counters(sysinf_t *from, sysinf_t *to)
{
	return EXIT_SUCCESS;
}

static inline iodev_inf_t* get_iodev(char *devname, iodev_inf_t *iodev_start)
{
	while(iodev_start != NULL) {
		if(strcmp(devname, iodev_start->dev) == 0) return iodev_start;
		iodev_start = iodev_start->next;
	}
	return NULL;
}

static inline iodev_inf_t* add_iodev(const char *devname, sysinf_t *sysinf)
{
	iodev_inf_t *iodev = sysinf->iodev;

	/* New linked-list */
	if(iodev == NULL) {
		sysinf->iodev = malloc(sizeof(iodev_inf_t));
		memset(sysinf->iodev, '\0', sizeof(iodev_inf_t));
		sysinf->iodev->dev = strdup(devname);
		return sysinf->iodev;
	}

	iodev_inf_t *workwithme = malloc(sizeof(iodev_inf_t));
	memset(workwithme, '\0', sizeof(iodev_inf_t));
	workwithme->dev = strdup(devname);

	/* ffwd to the end and tack it on */
	while(iodev->next != NULL) iodev = iodev->next;
	iodev->next = workwithme;
	workwithme->prev = iodev;
	return workwithme;
}

static inline void free_iodev(iodev_inf_t *iodev)
{
	if(iodev == NULL) return;

	/* Widow this obj */
	if(iodev->prev != NULL)
		((iodev_inf_t*)iodev->prev)->next = iodev->next;
	if(iodev->next != NULL)
		((iodev_inf_t*)iodev->next)->prev = iodev->prev;
	if(iodev->dev != NULL) free(iodev->dev);
	if(iodev->calc != NULL) free(iodev->calc);
	free(iodev);
}

static inline fsinf_t* get_fsdev(char *devname, fsinf_t *fsdev_start)
{
	while(fsdev_start != NULL) {
		if(strcmp(devname, fsdev_start->dev) == 0) return fsdev_start;
		fsdev_start = fsdev_start->next;
	}
	return NULL;
}

static inline fsinf_t* add_fsdev(const char *devname, sysinf_t *sysinf)
{
	fsinf_t *fsdev = sysinf->fsinf;

	/* New linked-list */
	if(fsdev == NULL) {
		sysinf->fsinf = malloc(sizeof(fsinf_t));
		memset(sysinf->fsinf, '\0', sizeof(fsinf_t));
		sysinf->fsinf->dev = strdup(devname);
		return sysinf->fsinf;
	}

	fsinf_t *workwithme = malloc(sizeof(fsinf_t));
	memset(workwithme, '\0', sizeof(fsinf_t));
	workwithme->dev = strdup(devname);

	/* ffwd to the end and tack it on */
	while(fsdev->next != NULL) fsdev = fsdev->next;
	fsdev->next = workwithme;
	workwithme->prev = fsdev;
	return workwithme;
}

static inline void free_fsdev(fsinf_t *fsdev)
{
	if(fsdev == NULL) return;

	/* Widow this obj */
	if(fsdev->prev != NULL)
		((fsinf_t*)fsdev->prev)->next = fsdev->next;
	if(fsdev->next != NULL)
		((fsinf_t*)fsdev->next)->prev = fsdev->prev;

	if(fsdev->dev != NULL) free(fsdev->dev);
	if(fsdev->fstype != NULL) free(fsdev->fstype);
	if(fsdev->mountpoint != NULL) free(fsdev->mountpoint);
	free(fsdev);
}

static inline iface_inf_t* get_iface(char *ifacedev, iface_inf_t *iface_start)
{
    while(iface_start != NULL) {
        if(strcmp(ifacedev, iface_start->dev) == 0) return iface_start;
        iface_start = iface_start->next;
    }
    return NULL;
}

static inline iface_inf_t* add_iface(const char *devname, sysinf_t *sysinf)
{
	iface_inf_t *iface = sysinf->iface;

    /* New linked-list */
    if(iface == NULL) {
    	sysinf->iface = malloc(sizeof(iface_inf_t));
        memset(sysinf->iface, '\0', sizeof(iface_inf_t));
        sysinf->iface->dev = strdup(devname);
        return sysinf->iface;
    }
    
    iface_inf_t *workwithme = malloc(sizeof(iface_inf_t));
	memset(workwithme, '\0', sizeof(iface_inf_t));
	workwithme->dev = strdup(devname);

	/* ffwd to the end and tack it on */
	while(iface->next != NULL) iface = iface->next;
	iface->next = workwithme;
	workwithme->prev = iface;
	return workwithme;
}

static inline void free_iface(iface_inf_t *iface)
{
	if(iface == NULL) return;

	/* Widow this obj */
	if(iface->prev != NULL)
		((iface_inf_t*)iface->prev)->next = iface->next;
	if(iface->next != NULL)
		((iface_inf_t*)iface->next)->prev = iface->prev;

	if(iface->dev != NULL) free(iface->dev);
	if(iface->calc != NULL) free(iface->calc);
	free(iface);
}

static inline cpu_inf_t* get_cpu(u_int cpuid, sysinf_t *sysinf)
{
	cpu_inf_t *cpus = sysinf->cpu;
    while(cpus != NULL) {
        if(cpus->cpu == cpuid) return cpus;
        cpus = cpus->next;
    }
    return NULL;
}

static inline cpu_inf_t* add_cpu(u_int cpuid, sysinf_t *sysinf)
{
    
	cpu_inf_t *cpus = sysinf->cpu;

    /* New linked list */
    if(cpus == NULL) {
    	sysinf->cpu = malloc(sizeof(cpu_inf_t));
        memset(sysinf->cpu, '\0', sizeof(cpu_inf_t));
        sysinf->cpu->cpu = cpuid;
        return sysinf->cpu;
    }

	cpu_inf_t *workwithme = malloc(sizeof(cpu_inf_t));
	memset(workwithme, '\0', sizeof(cpu_inf_t));
	workwithme->cpu = cpuid;

	/* Make the struct for data rate values */
	workwithme->calc = malloc(sizeof(cpu_inf_calc_t));
	memset(workwithme->calc, '\0', sizeof(cpu_inf_calc_t));

	/* ffwd to the end */
	while(cpus->next != NULL) cpus = cpus->next;
	cpus->next = workwithme;
	workwithme->prev = cpus;
	return workwithme;
}

static inline void free_cpu(cpu_inf_t *cpu)
{
    if(cpu == NULL) return;

	/* Widow this obj */
	if(cpu->prev != NULL)
		((cpu_inf_t*)cpu->prev)->next = cpu->next;
	if(cpu->next != NULL)
		((cpu_inf_t*)cpu->next)->prev = cpu->prev;

    if(cpu->flags != NULL) free(cpu->flags);
    if(cpu->vendor_id != NULL) free(cpu->vendor_id);
    if(cpu->model != NULL) free(cpu->model);
    if(cpu->calc != NULL) free(cpu->calc);
    if(cpu->cache_units != NULL) free(cpu->cache_units);
    free(cpu);
}

#define FREECPUS(cpus) {\
	while(cpus != NULL) {\
		ptr = cpus->next;\
		free_cpu(cpus);\
		cpus = ptr;\
	}\
}

#define FREEFSINF(fsinf) {\
	while(fsinf != NULL) {\
		ptr = fsinf->next;\
		free_fsdev(fsinf);\
		fsinf = ptr;\
	}\
}

#define FREEFIFACES(ifaces) {\
	while(ifaces != NULL) {\
		ptr = ifaces->next;\
		free_iface(ifaces);\
		ifaces = ptr;\
	}\
}

#define FREEFIODEVS(iodevs) {\
	while(iodevs != NULL) {\
		ptr = iodevs->next;\
		free_iodev(iodevs);\
		iodevs = ptr;\
	}\
}

inline void free_data_obj(sysinf_t *dobj)
{
	if(dobj == NULL) return;
	void *ptr; /* used in macros */
	if(dobj->calc != NULL) free(dobj->calc);
	if(dobj->cpu != NULL) FREECPUS(dobj->cpu);
	if(dobj->fsinf != NULL) FREEFSINF(dobj->fsinf);
	if(dobj->iface != NULL) FREEFIFACES(dobj->iface);
	if(dobj->iodev != NULL) FREEFIODEVS(dobj->iodev);

	if(dobj->hostname != NULL) free(dobj->hostname);
	free(dobj);
}


#define SET_J_STRING(root, index, dest) {\
    if( (tmpobj = json_object_get(root, index)) != NULL) {\
        dest = strdup(json_string_value(tmpobj));\
    }\
}

#define SET_J_REAL(root, index, dest) {\
    if( (tmpobj = json_object_get(root, index)) != NULL) {\
        *dest = json_real_value(tmpobj);\
    }\
}

#define SET_J_INTEGER(root, index, dest) {\
    if( (tmpobj = json_object_get(root, index)) != NULL) {\
        *dest = json_integer_value(tmpobj);\
    }\
}

inline static void cpustatic_from_json(const json_t* root, sysinf_t* target)
{
	json_t *tmparr = json_object_get(root, "cpus_static");
	json_t *tmproot;
	json_t *tmpobj;
	cpu_inf_t *cpu;
	size_t i;
	u_int indx;

	for(i = 0; tmparr != NULL && i < json_array_size(tmparr); i += 1) {

		tmproot = json_array_get(tmparr, i);

		/* snatch the CPU ID we need to update */
		if((tmpobj = json_object_get(tmproot, "cpu")) == NULL) continue;
		indx = json_integer_value(tmpobj);
		json_decref(tmpobj);

		/* get (or add) the cpu we're going to update */
		cpu = get_cpu(indx, target);
		if(cpu == NULL) cpu = add_cpu(indx, target);

		//SET_J_INTEGER(tmproot, "cpu", &cpu->cpu);
		SET_J_STRING(tmproot, "vendor_id", cpu->vendor_id);
		SET_J_STRING(tmproot, "model", cpu->model);
		SET_J_STRING(tmproot, "flags", cpu->flags);
		SET_J_INTEGER(tmproot, "cache", &cpu->cache);
		SET_J_STRING(tmproot, "cache_units", cpu->cache_units);
		SET_J_INTEGER(tmproot, "phy_id", &cpu->phy_id);
		SET_J_INTEGER(tmproot, "siblings", &cpu->siblings);
		SET_J_INTEGER(tmproot, "core_id", &cpu->core_id);
		SET_J_INTEGER(tmproot, "cpu_cores", &cpu->cpu_cores);
		SET_J_REAL(tmproot, "bogomips", &cpu->bogomips);

		json_decref(tmproot);
	}
	json_decref(tmparr);

}

inline static void cpudyn_from_json(const json_t* root, sysinf_t* target)
{
	json_t *jcpus = json_object_get(root, "cpus");
	json_t *jcpu;
	json_t *tmpobj;
	cpu_inf_t *cpu;
	size_t i, indx;

	for(i = 0; jcpus != NULL && i < json_array_size(jcpus); i += 1) {

		jcpu = json_array_get(jcpus, i);

		/* snatch the CPU ID we need to update */
		if((tmpobj = json_object_get(jcpu, "cpu")) == NULL) continue;
		indx = json_integer_value(tmpobj);
		json_decref(tmpobj);

		/* get (or add) the cpu we're going to update */
		cpu = get_cpu(indx, target);
		if(cpu == NULL) cpu = add_cpu(indx, target);

		SET_J_INTEGER(jcpu, "user", &cpu->user);
		SET_J_INTEGER(jcpu, "nice", &cpu->nice);
		SET_J_INTEGER(jcpu, "system", &cpu->system);
		SET_J_INTEGER(jcpu, "idle", &cpu->idle);
		SET_J_INTEGER(jcpu, "iowait", &cpu->iowait);
		SET_J_INTEGER(jcpu, "irq", &cpu->irq);
		SET_J_INTEGER(jcpu, "s_irq", &cpu->softirq);
		SET_J_INTEGER(jcpu, "steal", &cpu->steal);
		SET_J_INTEGER(jcpu, "guest", &cpu->guest);
		SET_J_INTEGER(jcpu, "n_guest", &cpu->guest_nice);
		SET_J_REAL(jcpu, "clock", &cpu->cpuMhz);

		json_decref(jcpu);
	}
	json_decref(jcpus);
}

#define J_GET_S(jobj, key) {\
	return strdup(json_string_value(json_object_get(jobj, key));\
}

#define J_GET_I(jobj, key) {\
	return json_integer_value(json_object_get(jobj, key));\
}

#define J_GET_R(jobj, key) {\
	return json_real_value(json_object_get(jobj, key));\
}


inline static cpu_inf_t* mkget_cpu(u_int cpu, sysinf_t *sysinf)
{
	cpu_inf_t *cpuobj = sysinf->cpu;
	while(cpuobj != NULL) {
		if ( cpuobj->cpu == cpu ) return cpuobj;
		cpuobj = cpuobj->next;
	}

	/* couldn't find it - NEW ONE!!!! */
	return add_cpu(cpu, sysinf);
}

sysinf_t* dataobj_from_json(json_t* msg)
{
	uint64_t id;
	size_t indx, cpu;
	sysinf_t *newobj = NULL;
	const char *strbuf = json_string_value(json_object_get(msg, "hostid"));
	json_t *obj, *arr;
	fsinf_t *fsinf;
	iodev_inf_t *iodevinf;
	iface_inf_t *ifaceinf;
	cpu_inf_t *cpuinf;

	if(strbuf == NULL) return NULL;

	sscanf(strbuf, "%lx", &id);

	newobj = new_data_obj(id);

	/* File system stats */
	arr = json_object_get(msg, "fs");
	if( json_is_array(arr) ) {
		json_array_foreach(arr, indx, obj) {
			strbuf = json_string_value(json_object_get(obj, "dev"));
			if ( (fsinf = add_fsdev(strbuf, newobj)) != NULL ) {
				fsinf->mountpoint = strdup(json_string_value(json_object_get(obj, "mountpoint")));
				fsinf->fstype = strdup(json_string_value(json_object_get(obj, "fstype")));
				fsinf->blks_total = json_integer_value(json_object_get(obj, "blks_total"));
				fsinf->blks_free = json_integer_value(json_object_get(obj, "blks_free"));
				fsinf->blks_avail = json_integer_value(json_object_get(obj, "blks_avail"));
				fsinf->block_size = json_integer_value(json_object_get(obj, "block_size"));
				fsinf->inodes_ttl = json_integer_value(json_object_get(obj, "inodes_ttl"));
				fsinf->inodes_free = json_integer_value(json_object_get(obj, "inodes_free"));
			}
		}
	}

	/* Network interface stats */
	arr = json_object_get(msg, "iface");
	if( json_is_array(arr) ) {
		json_array_foreach(arr, indx, obj) {
			strbuf = json_string_value(json_object_get(obj, "dev"));
			if ( (ifaceinf = add_iface(strbuf, newobj)) != NULL ) {
				ifaceinf->rx_packets = json_integer_value(json_object_get(obj, "rx_packets"));
				ifaceinf->rx_bytes = json_integer_value(json_object_get(obj, "rx_bytes"));
				ifaceinf->rx_errs = json_integer_value(json_object_get(obj, "rx_errs"));
				ifaceinf->rx_drop = json_integer_value(json_object_get(obj, "rx_drop"));
				ifaceinf->rx_fifo = json_integer_value(json_object_get(obj, "rx_fifo"));
				ifaceinf->rx_frame = json_integer_value(json_object_get(obj, "rx_frame"));
				ifaceinf->rx_comp = json_integer_value(json_object_get(obj, "rx_comp"));
				ifaceinf->rx_multi = json_integer_value(json_object_get(obj, "rx_multi"));
				ifaceinf->tx_packets = json_integer_value(json_object_get(obj, "tx_packets"));
				ifaceinf->tx_bytes = json_integer_value(json_object_get(obj, "tx_bytes"));
				ifaceinf->tx_errs = json_integer_value(json_object_get(obj, "tx_errs"));
				ifaceinf->tx_drop = json_integer_value(json_object_get(obj, "tx_drop"));
				ifaceinf->tx_fifo = json_integer_value(json_object_get(obj, "tx_fifo"));
				ifaceinf->tx_colls = json_integer_value(json_object_get(obj, "tx_colls"));
				ifaceinf->tx_carr = json_integer_value(json_object_get(obj, "tx_carr"));
				ifaceinf->tx_comp = json_integer_value(json_object_get(obj, "tx_comp"));
			}
		}
	}

	/* I/O devices */
	arr = json_object_get(msg, "iodev");
	if( json_is_array(arr) ) {
		json_array_foreach(arr, indx, obj) {
			strbuf = json_string_value(json_object_get(obj, "dev"));
			if ( (iodevinf = add_iodev(strbuf, newobj)) != NULL ) {
				iodevinf->reads = json_integer_value(json_object_get(obj, "reads"));
				iodevinf->read_sectors = json_integer_value(json_object_get(obj, "read_sectors"));
				iodevinf->reads_merged = json_integer_value(json_object_get(obj, "reads_merged"));
				iodevinf->msec_reading = json_integer_value(json_object_get(obj, "msec_reading"));
				iodevinf->writes = json_integer_value(json_object_get(obj, "writes"));
				iodevinf->write_sectors = json_integer_value(json_object_get(obj, "write_sectors"));
				iodevinf->writes_merged = json_integer_value(json_object_get(obj, "writes_merged"));
				iodevinf->msec_writing = json_integer_value(json_object_get(obj, "msec_writing"));
				iodevinf->current_ios = json_integer_value(json_object_get(obj, "current_ios"));
				iodevinf->msec_ios = json_integer_value(json_object_get(obj, "msec_ios"));
				iodevinf->weighted_ios = json_integer_value(json_object_get(obj, "weighted_ios"));
			}
		}
	}

	/* CPUS */
	arr = json_object_get(msg, "cpus");
	if ( json_is_array(arr) ) {
		json_array_foreach(arr, indx, obj) {
			cpu = json_integer_value(json_object_get(obj, "cpu"));
			if ( (cpuinf = mkget_cpu(cpu, newobj)) != NULL ) {
				cpuinf->user = json_integer_value(json_object_get(obj, "user"));
				cpuinf->nice = json_integer_value(json_object_get(obj, "nice"));
				cpuinf->system = json_integer_value(json_object_get(obj, "system"));
				cpuinf->idle = json_integer_value(json_object_get(obj, "idle"));
				cpuinf->iowait = json_integer_value(json_object_get(obj, "iowait"));
				cpuinf->irq = json_integer_value(json_object_get(obj, "irq"));
				cpuinf->softirq = json_integer_value(json_object_get(obj, "s_irq"));
				cpuinf->steal = json_integer_value(json_object_get(obj, "steal"));
				cpuinf->guest = json_integer_value(json_object_get(obj, "guest"));
				cpuinf->guest_nice = json_integer_value(json_object_get(obj, "n_guest"));
				cpuinf->cpuMhz = json_real_value(json_object_get(obj, "clock"));
			}
		}
	}

	arr = json_object_get(msg, "cpus_static");
	if ( json_is_array(arr) ) {
		json_array_foreach(arr, indx, obj) {
			cpu = json_integer_value(json_object_get(obj, "cpu"));
			if ( (cpuinf = mkget_cpu(cpu, newobj)) != NULL ) {
				cpuinf->vendor_id = strdup(json_string_value(json_object_get(obj, "vendor_id")));
				cpuinf->model = strdup(json_string_value(json_object_get(obj, "model")));
				cpuinf->flags = strdup(json_string_value(json_object_get(obj, "flags")));
				cpuinf->cache = json_integer_value(json_object_get(obj, "cache"));
				cpuinf->cache_units = strdup(json_string_value(json_object_get(obj, "cache_units")));
				cpuinf->phy_id = json_integer_value(json_object_get(obj, "phy_id"));
				cpuinf->siblings = json_integer_value(json_object_get(obj, "siblings"));
				cpuinf->core_id = json_integer_value(json_object_get(obj, "core_id"));
				cpuinf->cpu_cores = json_integer_value(json_object_get(obj, "cpu_cores"));
				cpuinf->bogomips = json_real_value(json_object_get(obj, "bogomips"));
			}
		}
	}

	obj = json_object_get(msg, "memory");
	if(obj != NULL) {
		newobj->mem_total = json_integer_value(json_object_get(obj, "total"));
		newobj->mem_free = json_integer_value(json_object_get(obj, "free"));
		newobj->mem_buffers = json_integer_value(json_object_get(obj, "buffers"));
		newobj->mem_cache = json_integer_value(json_object_get(obj, "cache"));
		newobj->swap_total = json_integer_value(json_object_get(obj, "swap_total"));
		newobj->swap_free = json_integer_value(json_object_get(obj, "swap_free"));

	}

	/* Sysload and procs */
	obj = json_object_get(msg, "sysload");
	if(obj != NULL) {
		newobj->load_1 = json_real_value(json_object_get(obj, "1min"));
		newobj->load_5 = json_real_value(json_object_get(obj, "5min"));
		newobj->load_15 = json_real_value(json_object_get(obj, "15min"));
		newobj->procs_running = json_real_value(json_object_get(obj, "procs_run"));
		newobj->procs_total = json_real_value(json_object_get(obj, "procs_total"));
	}

	/* Get CPU ttls */
	obj = json_object_get(msg, "cpu_ttls");
	if(obj != NULL) {
		newobj->cpu_user = json_integer_value(json_object_get(obj, "user"));
		newobj->cpu_nice = json_integer_value(json_object_get(obj, "nice"));
		newobj->cpu_system = json_integer_value(json_object_get(obj, "system"));
		newobj->cpu_idle = json_integer_value(json_object_get(obj, "idle"));
		newobj->cpu_iowait = json_integer_value(json_object_get(obj, "iowait"));
		newobj->cpu_irq = json_integer_value(json_object_get(obj, "irq"));
		newobj->cpu_sirq = json_integer_value(json_object_get(obj, "s_irq"));
		newobj->cpu_steal = json_integer_value(json_object_get(obj, "steal"));
		newobj->cpu_guest = json_integer_value(json_object_get(obj, "guest"));
		newobj->cpu_guest_nice = json_integer_value(json_object_get(obj, "n_guest"));
		newobj->interrupts = json_integer_value(json_object_get(obj, "intrps"));
		newobj->s_interrupts = json_integer_value(json_object_get(obj, "s_intrps"));
		newobj->s_interrupts = json_integer_value(json_object_get(obj, "s_intrps"));
		newobj->context_switches = json_integer_value(json_object_get(obj, "ctxt"));
	}

	obj = json_object_get(msg, "uptime");
	if(obj != NULL) newobj->uptime = json_real_value(obj);
	obj = json_object_get(msg, "idletime");
	if(obj != NULL) newobj->idletime = json_real_value(obj);

	return newobj;
}

int update_and_merge(sysinf_t *newobj, sysinf_t *oldobj)
{

}
