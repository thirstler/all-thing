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

iodev_inf_t* del_iodev(iodev_inf_t* iodev, char* devname)
{
    iodev_inf_t *first = iodev;
    iodev_inf_t *prev = iodev;

    while( iodev != NULL) {

        if( strcmp(iodev->dev, devname) == 0) {
            sysinf.iodev_count -= 1;
            if(iodev->dev != NULL) free(iodev->dev);

            LIST_UNLINK(iodev);

            break;
        }
        prev = iodev;
        iodev = iodev->next;
    }
    return first;
}

iodev_inf_t* push_iodev(iodev_inf_t* dev, char *devname)
{
    iodev_inf_t* first = dev;

    /* For an empty set */
    if (dev == NULL) {
        dev = malloc(sizeof(iodev_inf_t));
        first = dev;
    } else {
        while( dev->next != NULL) {
            if (strcmp(dev->dev, devname) == 0 ) return first;
            dev = dev->next;
        }
        dev->next = malloc(sizeof(iodev_inf_t));
        dev = dev->next;
    }

    /* Now working with the new device */
    memset(dev, 0, sizeof(iodev_inf_t));
    dev->dev = malloc(strlen(devname)+sizeof(char));
    strcpy(dev->dev, devname);
    dev->next = NULL;

    /* roll the counter */
    sysinf.iodev_count += 1;

    return first;
}

void dump_iodevdata(iodev_inf_t* iodev)
{
    while(iodev != NULL) {
        printf("--\nDevice: %s\n", iodev->dev);
        printf("  reads: %lu\n", iodev->reads);
        printf("    merged: %lu\n", iodev->reads_merged);
        printf("    sectors: %lu\n", iodev->read_sectors);
        printf("    msec: %lu\n", iodev->msec_reading);
        printf("  writes: %lu\n", iodev->writes);
        printf("    merged: %lu\n", iodev->writes_merged);
        printf("    sectors: %lu\n", iodev->write_sectors);
        printf("    msec: %lu\n", iodev->msec_writing);
        printf("  current I/Os: %lu\n", iodev->current_ios);
        printf("  ms I/Os: %lu\n", iodev->msec_ios);
        printf("  w I/Os: %lu\n", iodev->weighted_ios);
        iodev = iodev->next;
    }
}

inline void poll_iodev(iodev_inf_t* iodev)
{
    char statbuffer[PROC_DISKSTATS_BUFFER];
    char *statptr = NULL;
    FILE *fh = fopen("/proc/diskstats", "r");
    if(fh == NULL) {
        fprintf(stderr, "bad file read: /proc/diskstats\n");
        return;
    }
    fread(statbuffer, PROC_DISKSTATS_BUFFER, 1, fh);
    fclose(fh);

    while(iodev != NULL) {
        if( (statptr = strstr(statbuffer, iodev->dev)) != NULL) {
            sscanf(statptr, "%*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                    &iodev->reads,
                    &iodev->reads_merged,
                    &iodev->read_sectors,
                    &iodev->msec_reading,
                    &iodev->writes,
                    &iodev->writes_merged,
                    &iodev->write_sectors,
                    &iodev->msec_writing,
                    &iodev->current_ios,
                    &iodev->msec_ios,
                    &iodev->weighted_ios);
            iodev = iodev->next;
        } else {
            fprintf(stderr, "bad device name!\n");
            break;
        }
    }
}

iodev_inf_t* init_iodev(iodev_inf_t* iodev)
{
    char linebuffer[LINE_BUFFER_SZ];
    char strbuf[255];

    while(iodev != NULL) iodev = del_iodev(iodev, iodev->dev);

    FILE *fh = fopen("/proc/diskstats", "r");

    while( fgets(linebuffer, LINE_BUFFER_SZ, fh) != NULL) {
        sscanf(linebuffer, "%*d %*d %s ", strbuf);
        iodev = push_iodev(iodev, strbuf);
    }

    fclose(fh);

    return iodev;
}
