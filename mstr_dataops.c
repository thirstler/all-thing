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

inline sysinf_t* new_sysinf_obj(uint64_t newid)
{
    sysinf_t *newobj = malloc(sizeof(sysinf_t));
    memset(newobj, '\0', sizeof(sysinf_t));
    newobj->id = newid;
    return newobj;
}

inline void free_obj_rec(obj_rec_t *dobj)
{
	if(dobj == NULL) return;
	json_decref(dobj->record);
	free(dobj);
}

inline int rm_obj_rec(uint64_t id, master_global_data_t *data)
{
	size_t i;

	for(i = 0; i < data->obj_rec_sz; i += 1) {
		if(id == data->obj_rec[i]->id) {
			free_obj_rec(data->obj_rec[i]);
			data->obj_rec_sz -= 1;
			for(;i < data->obj_rec_sz; i += 1)
				data->obj_rec[i] = data->obj_rec[i+1];
			return EXIT_SUCCESS;
		}
	}
	return EXIT_SUCCESS;
}

inline obj_rec_t* get_obj_rec(uint64_t id, master_global_data_t *data)
{
	// Brute force for now, I'll get fancy later
	size_t i;
	for(i=0; i < data->obj_rec_sz; i += 1) {
		if(id == data->obj_rec[i]->id) {
			syslog(LOG_DEBUG, "get_data_obj() found id %lx", id);
			return data->obj_rec[i];
		}
	}
	syslog(LOG_DEBUG, "no object with id %lx in store", id);
	return NULL;
}

inline int add_obj_rec(master_global_data_t *master, obj_rec_t *newobj)
{
	size_t i;

	if(newobj->id==0) return EXIT_FAILURE;

	if( master->obj_rec_sz >= DATA_STRUCT_AR_SIZE) {
		syslog(LOG_ERR, "data buffer full, increase to > %d",
				DATA_STRUCT_AR_SIZE);
		return EXIT_FAILURE;
	}

	for(i=0; i < master->obj_rec_sz; i += 1) {
		if( newobj->id == master->obj_rec[i]->id) return EXIT_FAILURE;
	}

	master->obj_rec[master->obj_rec_sz] = newobj;
	master->obj_rec_sz += 1;

	syslog(LOG_DEBUG, "add_new_data_obj() added %lx, new size %lu\n",
			 newobj->id, master->obj_rec_sz);

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

#define rate_from_jsonobjs(key, curobj, newobj, target, tdiff) {\
	json_object_set(\
		target,\
		key,\
		json_real(\
			(float)(json_integer_value(json_object_get(newobj, key)) -\
			json_integer_value(json_object_get(curobj, key)))/\
			((float) tdiff.tv_sec +\
			((float) tdiff.tv_usec/1000000))));\
}

int calc_data_rates(json_t *standing, json_t *incomming)
{
	struct timeval diff, standing_ts, incomming_ts;
	json_t *ts;
	float rate;
	json_t *tmp_st, *tmp_in, *tmp_calc, *tmp_arr_st, *tmp_arr_in;
	size_t i;

	if ((ts = json_object_get(standing, "ts")) == NULL) return EXIT_FAILURE;
	standing_ts.tv_sec = json_integer_value(json_object_get(ts, "tv_sec"));
	standing_ts.tv_usec = json_integer_value(json_object_get(ts, "tv_usec"));

	if ((ts = json_object_get(incomming, "ts")) == NULL) return EXIT_FAILURE;
	incomming_ts.tv_sec = json_integer_value(json_object_get(ts, "tv_sec"));
	incomming_ts.tv_usec = json_integer_value(json_object_get(ts, "tv_usec"));

	timersub(&incomming_ts, &standing_ts, &diff);

	/* Set rate for CPU totals (cpu_ttls) ************************************/
	tmp_in = json_object_get(incomming, "cpu_ttls");
	tmp_st = json_object_get(standing, "cpu_ttls");
	tmp_calc = json_object_get(tmp_st, "__rates");
	if(tmp_calc == NULL)
		json_object_set(tmp_st, "__rates", tmp_calc = json_object());
	rate_from_jsonobjs("user", tmp_st, tmp_in, tmp_calc, diff);
	rate_from_jsonobjs("nice", tmp_st, tmp_in, tmp_calc, diff);
	rate_from_jsonobjs("system", tmp_st, tmp_in, tmp_calc, diff);
	rate_from_jsonobjs("idle", tmp_st, tmp_in, tmp_calc, diff);
	rate_from_jsonobjs("iowait", tmp_st, tmp_in, tmp_calc, diff);
	rate_from_jsonobjs("irq", tmp_st, tmp_in, tmp_calc, diff);
	rate_from_jsonobjs("s_irq", tmp_st, tmp_in, tmp_calc, diff);
	rate_from_jsonobjs("steal", tmp_st, tmp_in, tmp_calc, diff);
	rate_from_jsonobjs("guest", tmp_st, tmp_in, tmp_calc, diff);
	rate_from_jsonobjs("n_guest", tmp_st, tmp_in, tmp_calc, diff);
	rate_from_jsonobjs("intrps", tmp_st, tmp_in, tmp_calc, diff);
	rate_from_jsonobjs("s_intrps", tmp_st, tmp_in, tmp_calc, diff);
	rate_from_jsonobjs("ctxt", tmp_st, tmp_in, tmp_calc, diff);

	/* Set rates for individual CPUS *****************************************/
	tmp_arr_in = json_object_get(incomming, "cpus");
	tmp_arr_st = json_object_get(standing, "cpus");

	for(i=0; i < json_array_size(tmp_arr_in); i += 1) {
		tmp_in = json_array_get(tmp_arr_in, i);
		tmp_st = json_array_get(tmp_arr_st, i);
		tmp_calc = json_object_get(tmp_st, "__rates");
		if(tmp_calc == NULL)
			json_object_set(tmp_st, "__rates", tmp_calc = json_object());

		rate_from_jsonobjs("user", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("nice", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("system", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("idle", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("iowait", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("irq", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("s_irq", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("steal", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("guest", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("n_guest", tmp_st, tmp_in, tmp_calc, diff);
	}

	/* Set rates for network counters ****************************************/
	tmp_arr_in = json_object_get(incomming, "iface");
	tmp_arr_st = json_object_get(standing, "iface");
	for(i=0; i < json_array_size(tmp_arr_in); i += 1) {
		tmp_in = json_array_get(tmp_arr_in, i);
		tmp_st = json_array_get(tmp_arr_st, i);
		tmp_calc = json_object_get(tmp_st, "__rates");
		if(tmp_calc == NULL)
			json_object_set(tmp_st, "__rates", tmp_calc = json_object());

		rate_from_jsonobjs("rx_packets", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("rx_bytes", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("rx_errs", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("rx_drop", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("rx_fifo", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("rx_frame", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("rx_comp", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("rx_multi", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("tx_packets", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("tx_bytes", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("tx_errs", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("tx_drop", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("tx_fifo", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("tx_colls", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("tx_carr", tmp_st, tmp_in, tmp_calc, diff);
		rate_from_jsonobjs("tx_comp", tmp_st, tmp_in, tmp_calc, diff);
	}
	printf("%s\n", json_dumps(standing, JSON_INDENT(2)));

	return EXIT_SUCCESS;
}
