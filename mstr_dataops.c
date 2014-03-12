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


sysinf_t* get_sysinf(unsigned int id, sysinf_t **data_map)
{
    return data_map[0];
}

static inline int comp_ids(const void *p1, const void *p2)
{
    return ((sysinf_t*)p2)->id - ((sysinf_t*)p1)->id;
}


sysinf_t** map_sysinf_data(
        sysinf_t *system_data,
        sysinf_t **data_map)
{
    size_t count = 0;
    sysinf_t *sdp;
    sdp = system_data;
    for(count=0; sdp != NULL; count += 1) sdp = sdp->next;

    data_map = realloc(data_map, sizeof(sysinf_t*)*count);

    sdp = system_data;
    for(count=0; sdp != NULL; count += 1) {
        data_map[count] = sdp;
        sdp = sdp->next;
    }

    qsort(&data_map[0], count-1, sizeof(sysinf_t*), comp_ids);

    return data_map;
}

static sysinf_t* new_data_obj(unsigned int newid)
{
    sysinf_t *newobj = malloc(sizeof(sysinf_t));
    memset(newobj, '\0', sizeof(sysinf_t));
    newobj->id = newid;
    return newobj;
}

static cpu_inf_t* new_cpu(cpu_inf_t *cpu)
{
    if(cpu == NULL) {
        return malloc(sizeof(cpu_inf_t));
    }
    while(cpu->next != NULL) cpu = cpu->next;

    cpu->next = malloc(sizeof(cpu_inf_t));
    return cpu->next;
}

static void free_cpu(cpu_inf_t *cpu)
{
    if(cpu == NULL) return;
    if(cpu->flags != NULL) free(cpu->flags);
    if(cpu->vendor_id != NULL) free(cpu->vendor_id);
    if(cpu->model != NULL) free(cpu->vendor_id);
    free(cpu);
}

static void free_cpus(cpu_inf_t *cpus)
{
    cpu_inf_t *ptr;
    while(cpus != NULL) {
        ptr = cpus->next;
        free_cpu(cpus);
        cpus = ptr;
    }
}

#defint SET_J_STRING(root, index, dest) {\
    if( (tmpobj = json_object_get(root, index)) != NULL) {\
        if(dest != NULL) free(dest);
        dest = strdup(json_string_value(tmpobj);\
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

sysinf_t* dataobj_from_jsonstr(
        unsigned int id, char *jsontxt, sysinf_t *target)
{

    json_error_t error;
    json_t *root = json_loads(jsontxt, 0, &error);
    json_t *tmproot, *tmpobj, *tmparr;
    cpu_inf_t *cpu;
    size_t i;

    if(root == NULL) return NULL;

    /* New object rather than an update */
    if(target == NULL) {
        target = new_data_obj(id);
    }

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
        SET_J_INTEGER(tmproot, "user", &target->cpu_user);{
        SET_J_INTEGER(tmproot, "system", &target->cpu_system);
        SET_J_INTEGER(tmproot, "nice", &target->cpu_nice);
        SET_J_INTEGER(tmproot, "iowait", &target->cpu_iowait);
        SET_J_INTEGER(tmproot, "idle", &target->cpu_idle);
        SET_J_INTEGER(tmproot, "steal", &target->cpu_steal);
        SET_J_INTEGER(tmproot, "guest", &target->cpu_guest);
        SET_J_INTEGER(tmproot, "n_guest", &target->cpu_guest_nice);
        SET_J_INTEGER(tmproot, "irq", &target->cpu_irq);
        SET_J_INTEGER(tmproot, "s_irq", &target->cpu_sirq);
        SET_J_INTEGER(tmproot, "intrps", &target->interrupts);
        SET_J_INTEGER(tmproot, "s_intrps", &target->s_interrupts);
        json_decref(tmproot);
    }

    cpu = target->cpu;
    json_array_foreach(json_object_get(root, "cpus"), i, tmproot) {
        if(cpu != NULL) {
            SET_J_INTEGER(tmproot, "user", &cpu);
        }
    }
    return target;
}

void add_new_data_obj(
        sysinf_t *new_data_obj,
        sysinf_t **system_data)
{
}
