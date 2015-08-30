/*
 * File: cpuops.c
 * Desc: Functions for polling and calculating CPU related info on Linux
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

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <stdint.h>
#include <sys/types.h>
#include "../at.h"
#include "at_agent.h"

extern agent_sysstats_t sysinf;

cpu_inf_t* del_cpu(cpu_inf_t* cpu, u_int proc)
{
    cpu_inf_t *first = cpu;
    cpu_inf_t *prev = cpu;

    while( cpu != NULL) {

        if( cpu->cpu == proc ) {
            sysinf.cpu_count -= 1;
            if(cpu->flags != NULL) free(cpu->flags);
            if(cpu->model != NULL) free(cpu->model);
            if(cpu->vendor_id != NULL) free(cpu->vendor_id);
            if(cpu->cache_units != NULL) free(cpu->cache_units);
            LIST_UNLINK(cpu);

            break;
        }
        prev = cpu;
        cpu = cpu->next;
    }
    return first;
}

char* get_cpuinfstr(char* buffer)
{
    char *ptr;
    if( (ptr = strchr(buffer, ':')) != NULL) {
        ptr += 2;
        return ptr;
    }
    return NULL;
}

/* Just dups the string and overwrites the newline char */
#define SET_CPUINF_STR_VAL(tgt, src) {\
    tgt = strdup(src);\
    tgt[strlen(src)-1] = '\0';\
}

void init_proc(cpu_inf_t* cpu)
{
    FILE *fh;
    char linebuf[LINE_BUFFER_SZ];
    u_int intbuf = -1;
    u_int working = -1;

    fh = fopen("/proc/cpuinfo", "r");

    while( fgets(linebuf, LINE_BUFFER_SZ,  fh) ) {
        if( sscanf(linebuf, "processor : %u", &intbuf) == 1) {
            working = intbuf;
        }
        if(working != cpu->cpu) continue;

        if( strstr(linebuf, "vendor_id") != NULL )
            SET_CPUINF_STR_VAL(cpu->vendor_id, get_cpuinfstr(linebuf));

        if( strstr(linebuf, "model name") != NULL )
            SET_CPUINF_STR_VAL(cpu->model, get_cpuinfstr(linebuf));

        if( strstr(linebuf, "flags") != NULL )
            SET_CPUINF_STR_VAL(cpu->flags, get_cpuinfstr(linebuf));

        if(cpu->cache_units == NULL) cpu->cache_units = malloc(sizeof(char)*4);
        sscanf(linebuf, "cache size : %u %s", &cpu->cache, cpu->cache_units);
        sscanf(linebuf, "physical id : %u", &cpu->phy_id);
        sscanf(linebuf, "siblings : %u", &cpu->siblings);
        sscanf(linebuf, "core id : %u", &cpu->core_id);
        sscanf(linebuf, "cpu cores : %u", &cpu->cpu_cores);
        sscanf(linebuf, "bogomips : %f", &cpu->bogomips);

    }
    fclose(fh);
}

cpu_inf_t* push_cpu(cpu_inf_t* cpu, u_int proc)
{
    cpu_inf_t* first = cpu;

    /* For an empty set */
    if (cpu == NULL) {
        cpu = malloc(sizeof(cpu_inf_t));
        first = cpu;
    } else {
        while( cpu->next != NULL) {
            if( cpu->cpu == proc) return first;
            cpu = cpu->next;
        }
        cpu->next = malloc(sizeof(cpu_inf_t));
        cpu = cpu->next;
    }

    /* Now working with the new CPU */
    memset(cpu, 0, sizeof(cpu_inf_t));
    cpu->cpu = proc;
    cpu->next = NULL;

    init_proc(cpu);

    /* roll the counter */
    sysinf.cpu_count += 1;

    return first;
}

cpu_inf_t* init_cpu(cpu_inf_t* cpu)
{
    FILE *fh;
    char linebuf[LINE_BUFFER_SZ];
    u_int intbuf;

    while(cpu != NULL) cpu = del_cpu(cpu, cpu->cpu);

    memset(linebuf, '\0', LINE_BUFFER_SZ);

    fh = fopen("/proc/cpuinfo", "r");
    if (fh == NULL) {
        perror("fopen() on cpuinfo failed");
        return NULL;
    }

    while( fgets(linebuf, LINE_BUFFER_SZ,  fh)) {
        if( sscanf(linebuf, "processor : %u", &intbuf) == 1) {
            cpu = push_cpu(cpu, intbuf);
        }
    }
    fclose(fh);

    return cpu;
}

#ifdef DEBUG
void dump_cpudata(sysinf_t *host_data)
{
    cpu_inf_t* cpus = host_data->cpu;
    while(cpus != NULL) {
        printf("Individual CPU: %u\n", cpus->cpu);
        printf("  vendor:     %s\n", cpus->vendor_id);
        printf("  model:      %s\n", cpus->model);
        printf("  bogomips:   %f\n", cpus->bogomips);
        printf("  clock:      %f\n", cpus->cpuMhz);
        printf("  user:       %lu\n", cpus->user);
        printf("  nice:       %lu\n", cpus->nice);
        printf("  system:     %lu\n", cpus->system);
        printf("  idle:       %lu\n", cpus->idle);
        printf("  iowait:     %lu\n", cpus->iowait);
        printf("  irqs:       %lu\n", cpus->irq);
        printf("  soft:       %lu\n", cpus->softirq);
        printf("  steal:      %lu\n", cpus->steal);
        printf("  guest:      %lu\n", cpus->guest);
        printf("  guest nice: %lu\n", cpus->guest_nice);
        cpus = cpus->next;
    }

    printf("--\nCPU Totals\n");
    printf("  user:       %lu\n", host_data->cpu_user);
    printf("  nice:       %lu\n", host_data->cpu_nice);
    printf("  system:     %lu\n", host_data->cpu_system);
    printf("  idle:       %lu\n", host_data->cpu_idle);
    printf("  iowait:     %lu\n", host_data->cpu_iowait);
    printf("  irqs:       %lu\n", host_data->cpu_irq);
    printf("  soft:       %lu\n", host_data->cpu_sirq);
    printf("  steal:      %lu\n", host_data->cpu_steal);
    printf("  guest:      %lu\n", host_data->cpu_guest);
    printf("  guest nice: %lu\n", host_data->cpu_guest_nice);
}
#endif

void poll_cpus(sysinf_t *host_data)
{
    cpu_inf_t* cpus = host_data->cpu;
    cpu_inf_t* first = cpus;
    u_int detec = 0;
    FILE *fh;
    char procbuffer[(PROC_CPU_BUFFER>PROC_STAT_BUFFER)?PROC_CPU_BUFFER:PROC_STAT_BUFFER];
    char readbuf[LINE_BUFFER_SZ];
    char *tok = NULL;

    memset(procbuffer, '\0', PROC_CPU_BUFFER);

    /* Get CPU clock speeds */
    fh = fopen("/proc/cpuinfo", "r");
    fread(procbuffer, PROC_CPU_BUFFER, 1, fh);
    fclose(fh);

    tok = procbuffer;
    while( (tok = strstr(tok, "cpu MHz")) != NULL) {
        sscanf(tok, "cpu MHz : %f", &cpus->cpuMhz);
        tok += 11; // Move passed the found substring
        if(cpus->next == NULL) break;
        cpus = cpus->next;
    }

    /* Get CPU stat data *****************************************************/

    /* CPUs are the first lines of /proc/stat, assuming that position */
    detec = -1;
    fh = fopen("/proc/stat", "r");
    cpus = first;

    fgets(readbuf, LINE_BUFFER_SZ, fh);

    sscanf(readbuf, "%*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
        &host_data->cpu_user,
        &host_data->cpu_nice,
        &host_data->cpu_system,
        &host_data->cpu_idle,
        &host_data->cpu_iowait,
        &host_data->cpu_irq,
        &host_data->cpu_sirq,
        &host_data->cpu_steal,
        &host_data->cpu_guest,
        &host_data->cpu_guest_nice);

    while(fgets(readbuf, LINE_BUFFER_SZ, fh) != NULL) {

        sscanf(readbuf, "%*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
            &cpus->user,
            &cpus->nice,
            &cpus->system,
            &cpus->idle,
            &cpus->iowait,
            &cpus->irq,
            &cpus->softirq,
            &cpus->steal,
            &cpus->guest,
            &cpus->guest_nice);

        cpus = cpus->next;
        if (cpus == NULL ) break;
        detec += 1;
    }
    fread(procbuffer, PROC_STAT_BUFFER, 1, fh);
    tok = strstr(procbuffer, "intr ");
    sscanf(tok, "%*s %lu", &host_data->interrupts);
    tok = strstr(tok, "ctxt ");
    sscanf(tok, "%*s %lu", &host_data->context_switches);
    tok = strstr(tok, "softirq ");
    sscanf(tok, "%*s %lu", &host_data->s_interrupts);
    fclose(fh);
}
