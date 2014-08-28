#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "at.h"

extern agent_sysstats_t sysinf;

void set_semistatic_iface_inf(iface_inf_t* iface)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct ifreq ifr;

    strcpy(iface->ipv4_addr, "not implemented");
    strcpy(iface->macaddr, "not implemented");

    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, iface->dev);

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        close(fd);
        return;
    }
    close(fd);

}

iface_inf_t* push_iface(iface_inf_t* iface, char* devname)
{
    iface_inf_t* first = iface;

    if(iface == NULL) {
        iface = malloc(sizeof(iface_inf_t));
        first = iface;
    } else {
        while( iface->next != NULL) {
            if( strcmp(iface->dev, devname) == 0) return first;
            iface = iface->next;
        }
        iface->next = malloc(sizeof(iface_inf_t));
        iface = iface->next;
    }

    /* Now working with the new iface */
    memset(iface, 0, sizeof(iface_inf_t));
    iface->dev = malloc(sizeof(devname)+sizeof(char));
    strcpy(iface->dev, devname);
    iface->next = NULL;

    set_semistatic_iface_inf(iface);

    /* roll the counter */
    sysinf.iface_count += 1;

    return first;
}

iface_inf_t* del_iface(iface_inf_t* iface, char* devname)
{
    iface_inf_t *first = iface;
    iface_inf_t *prev = iface;

    while( iface != NULL) {

        if( strcmp(iface->dev, devname) == 0) {
            sysinf.iface_count -= 1;
            if(iface->dev != NULL) free(iface->dev);

            LIST_UNLINK(iface);

            break;
        }
        prev = iface;
        iface = iface->next;
    }
    return first;
}


void poll_iface(iface_inf_t* iface)
{
    char statbuffer[PROC_NETDEVSTATS_BUFFER];
    char *statptr;
    FILE *fh = fopen("/proc/net/dev", "r");
    if(fh == NULL) {
        fprintf(stderr, "bad file read: /proc/net/dev\n");
        return;
    }
    fread(statbuffer, PROC_NETDEVSTATS_BUFFER, 1, fh);
    fclose(fh);

    while(iface != NULL) {
        if( (statptr = strstr(statbuffer, iface->dev)) != NULL) {
            sscanf(statptr, "%*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                    &iface->rx_bytes,
                    &iface->rx_packets,
                    &iface->rx_errs,
                    &iface->rx_drop,
                    &iface->rx_fifo,
                    &iface->rx_frame,
                    &iface->rx_comp,
                    &iface->rx_multi,
                    &iface->tx_bytes,
                    &iface->tx_packets,
                    &iface->rx_errs,
                    &iface->tx_drop,
                    &iface->tx_fifo,
                    &iface->tx_colls,
                    &iface->tx_carr,
                    &iface->tx_comp);
            iface = iface->next;
        } else {
            fprintf(stderr, "bad device name!\n");
            break;
        }
    }
}

void dump_ifacedata(iface_inf_t* iface)
{
    while(iface != NULL) {
        printf("--\nDev: %s\n", iface->dev);
        printf("  Transmit --\n");
        printf("    bytes:   %lu\n", iface->tx_bytes);
        printf("    packets: %lu\n", iface->tx_packets);
        printf("    errors:  %lu\n", iface->tx_errs);
        printf("    drop:    %lu\n", iface->tx_drop);
        printf("    fifo:    %lu\n", iface->tx_fifo);
        printf("    colls:   %lu\n", iface->tx_colls);
        printf("    carrier: %lu\n", iface->tx_carr);
        printf("    comp:    %lu\n", iface->tx_comp);
        printf("  Receive --\n");
        printf("    bytes:   %lu\n", iface->rx_bytes);
        printf("    packets: %lu\n", iface->rx_packets);
        printf("    errors:  %lu\n", iface->rx_errs);
        printf("    drop:    %lu\n", iface->rx_drop);
        printf("    fifo:    %lu\n", iface->rx_fifo);
        printf("    frame:   %lu\n", iface->rx_frame);
        printf("    comp:    %lu\n", iface->rx_comp);
        printf("    multi:   %lu\n", iface->rx_multi);
        iface = iface->next;
    }
}

iface_inf_t* init_iface(iface_inf_t* iface)
{
    FILE *fh;
    char linebuf[LINE_BUFFER_SZ];
    char strbuf[STR_BUFFER_SZ];
    int linetrack = 0;

    while(iface != NULL) iface = del_iface(iface, iface->dev);

    fh = fopen("/proc/net/dev", "r");
    while( fgets(linebuf, LINE_BUFFER_SZ, fh) != NULL ) {
        if( linetrack < 2 ) { linetrack += 1; continue; } // Skip the header
        sscanf(linebuf, "%s", strbuf);
        *strchr(strbuf, ':') = '\0';
        iface = push_iface(iface, strbuf);
    }

    fclose(fh);

    return iface;
}
