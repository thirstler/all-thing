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

static inline iodev_inf_t* add_iodev(char *devname, sysinf_t *sysinf)
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

static inline fsinf_t* add_fsdev(char *devname, sysinf_t *sysinf)
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

static inline iface_inf_t* add_iface(char *devname, sysinf_t *sysinf)
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

static inline cpu_inf_t* add_cpu(size_t cpuid, sysinf_t *sysinf)
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
        if(dest != NULL) free(dest);\
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

sysinf_t* dataobj_from_json(uint64_t id, json_t* root, sysinf_t* target)
{

    json_t *tmproot, *tmpobj, *tmparr;
    iface_inf_t *iface;
    iodev_inf_t *iodev;
    fsinf_t *fsdev;
    size_t i;
    char *strbuf;
    if(target == NULL) target = new_data_obj(id);

    /* If it's not working JSON or our target doen't exit, fail */
    if(root == NULL || target == NULL) return NULL;

    /* These guys float right off of the root object */
    SET_J_STRING(root, "hostname", target->hostname);
    SET_J_REAL(root, "uptime", &target->uptime);
    SET_J_REAL(root, "idletime", &target->idletime);

    /* Grab the time-stamp */
    if( (tmproot = json_object_get(root, "ts")) != NULL) {
        SET_J_INTEGER(tmproot, "tv_sec", &target->sample_tv.tv_sec);
        SET_J_INTEGER(tmproot, "tv_usec", &target->sample_tv.tv_usec);
        json_decref(tmproot);
    }

    /* Memory info */
    if( (tmproot = json_object_get(root, "memory")) != NULL) {
        SET_J_INTEGER(tmproot, "cache", &target->mem_cache);
        SET_J_INTEGER(tmproot, "free", &target->mem_free);
        SET_J_INTEGER(tmproot, "total", &target->mem_total);
        SET_J_INTEGER(tmproot, "buffers", &target->mem_buffers);
        SET_J_INTEGER(tmproot, "swap_total", &target->swap_total);
        SET_J_INTEGER(tmproot, "swap_free", &target->swap_free);
        json_decref(tmproot);
    }

    /* Load info */
    if( (tmproot = json_object_get(root, "sysload")) != NULL) {
        SET_J_REAL(tmproot, "1min", &target->load_1);
        SET_J_REAL(tmproot, "5min", &target->load_5);
        SET_J_REAL(tmproot, "15min", &target->load_15);
        SET_J_INTEGER(tmproot, "procs_run", &target->procs_running);
        SET_J_INTEGER(tmproot, "procs_total", &target->procs_total);
        json_decref(tmproot);
    }

    /* CPU totals and system totals */
    if( (tmproot = json_object_get(root, "cpu_ttls")) != NULL) {
        SET_J_INTEGER(tmproot, "user", &target->cpu_user);
        SET_J_INTEGER(tmproot, "system", &target->cpu_system);
        SET_J_INTEGER(tmproot, "nice", &target->cpu_nice);
        SET_J_INTEGER(tmproot, "iowait", &target->cpu_iowait);
        SET_J_INTEGER(tmproot, "idle", &target->cpu_idle);
        SET_J_INTEGER(tmproot, "steal", &target->cpu_steal);
        SET_J_INTEGER(tmproot, "guest", &target->cpu_guest);
        SET_J_INTEGER(tmproot, "n_guest", &target->cpu_guest_nice);
        SET_J_INTEGER(tmproot, "irq", &target->cpu_irq);
        SET_J_INTEGER(tmproot, "s_irq", &target->cpu_sirq);
        SET_J_INTEGER(tmproot, "ctxt", &target->context_switches);
        SET_J_INTEGER(tmproot, "intrps", &target->interrupts);
        SET_J_INTEGER(tmproot, "s_intrps", &target->s_interrupts);
        json_decref(tmproot);
    }
    
    /* Static CPU info */
    cpustatic_from_json(root, target);
    
    /* Dynamic CPU info */
    cpudyn_from_json(root, target);
    
    /* Network Interfaces */
    
    tmparr = json_object_get(root, "iface");
    for(i=0; i < json_array_size(tmparr); i+=1) {
        
        tmproot = json_array_get(tmparr, i);

        /* snatch the CPU ID we need to update */
        if((tmpobj = json_object_get(tmproot, "dev")) == NULL) {
			json_decref(tmproot);
			continue;
		}
        strbuf = strdup(json_string_value(tmpobj));
        json_decref(tmpobj);
        
        /* get (or add) the iface we're going to update */
        if((iface = get_iface(strbuf, target->iface)) == NULL)
            iface = add_iface(strbuf, target);
        free(strbuf);
        
        SET_J_STRING(tmproot, "dev", iface->dev);
        SET_J_INTEGER(tmproot, "rx_packets", &iface->recv_packets);
        SET_J_INTEGER(tmproot, "rx_bytes", &iface->recv_bytes);
        SET_J_INTEGER(tmproot, "rx_errs", &iface->recv_errs);
        SET_J_INTEGER(tmproot, "rx_drop", &iface->recv_drop);
        SET_J_INTEGER(tmproot, "rx_fifo", &iface->recv_fifo);
        SET_J_INTEGER(tmproot, "rx_frame", &iface->recv_frame);
        SET_J_INTEGER(tmproot, "rx_comp", &iface->recv_compressed);
        SET_J_INTEGER(tmproot, "rx_multi", &iface->recv_multicasts);
        SET_J_INTEGER(tmproot, "tx_packets", &iface->trns_packets);
        SET_J_INTEGER(tmproot, "tx_bytes", &iface->trns_bytes);
        SET_J_INTEGER(tmproot, "tx_errs", &iface->trns_errs);
        SET_J_INTEGER(tmproot, "tx_drop", &iface->trns_drop);
        SET_J_INTEGER(tmproot, "tx_fifo", &iface->trns_fifo);
        SET_J_INTEGER(tmproot, "tx_colls", &iface->trns_colls);
        SET_J_INTEGER(tmproot, "tx_carr", &iface->trns_carrier);
        SET_J_INTEGER(tmproot, "tx_comp", &iface->trns_compressed);
        
        json_decref(tmproot);
    }
    json_decref(tmparr);
   
    /* I/O devices */

    tmparr = json_object_get(root, "iodev");
    for(i=0; i < json_array_size(tmparr); i+=1) {

		tmproot = json_array_get(tmparr, i);

    	/* snatch the device ID we need to update */
		if((tmpobj = json_object_get(tmproot, "dev")) == NULL) {
			json_decref(tmproot);
			continue;
		}
		strbuf = strdup(json_string_value(tmpobj));
		json_decref(tmpobj);

		/* get (or add) the iface we're going to update */
		if((iodev = get_iodev(strbuf, target->iodev)) == NULL)
			iodev = add_iodev(strbuf, target);
		free(strbuf);

		SET_J_STRING(tmproot, "dev", iodev->dev);
		SET_J_INTEGER(tmproot, "reads", &iodev->reads);
		SET_J_INTEGER(tmproot, "read_sectors", &iodev->read_sectors);
		SET_J_INTEGER(tmproot, "reads_merged", &iodev->reads_merged);
		SET_J_INTEGER(tmproot, "msec_reading", &iodev->msec_reading);
		SET_J_INTEGER(tmproot, "writes", &iodev->writes);
		SET_J_INTEGER(tmproot, "write_sectors", &iodev->write_sectors);
		SET_J_INTEGER(tmproot, "writes_merged", &iodev->writes_merged);
		SET_J_INTEGER(tmproot, "msec_writing", &iodev->msec_writing);
		SET_J_INTEGER(tmproot, "current_ios", &iodev->current_ios);
		SET_J_INTEGER(tmproot, "msec_ios", &iodev->msec_ios);
		SET_J_INTEGER(tmproot, "weighted_ios", &iodev->weighted_ios);

		json_decref(tmproot);

    }
    json_decref(tmparr);

    /* File systems */
    tmparr = json_object_get(root, "fs");
    for(i=0; i < json_array_size(tmparr); i+=1) {

		tmproot = json_array_get(tmparr, i);

		/* snatch the device ID we need to update */
		if((tmpobj = json_object_get(tmproot, "dev")) == NULL) {
			json_decref(tmproot);
			continue;
		}

		strbuf = strdup(json_string_value(tmpobj));
		json_decref(tmpobj);

		/* get (or add) the iface we're going to update */
		if((fsdev = get_fsdev(strbuf, target->fsinf)) == NULL)
			fsdev = add_fsdev(strbuf, target);
		free(strbuf);

		SET_J_STRING(tmproot, "dev", fsdev->dev);
		SET_J_STRING(tmproot, "mountpoint", fsdev->mountpoint);
		SET_J_STRING(tmproot, "fstype", fsdev->fstype);
		SET_J_INTEGER(tmproot, "blks_total", &fsdev->blks_total);
		SET_J_INTEGER(tmproot, "blks_free", &fsdev->blks_free);
		SET_J_INTEGER(tmproot, "blks_avail", &fsdev->blks_avail);
		SET_J_INTEGER(tmproot, "block_size", &fsdev->block_size);
		SET_J_INTEGER(tmproot, "inodes_ttl", &fsdev->inodes_ttl);
		SET_J_INTEGER(tmproot, "inodes_free", &fsdev->inodes_free);

		json_decref(tmproot);

	}
	json_decref(tmparr);

	json_decref(root);

#ifdef DEBUG
	dump_dataobj(target);
#endif

    return target;
}
