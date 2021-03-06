/*
 * File: fsops.c
 * Desc: Functions for polling file system related info on Linux
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
#include <sys/vfs.h>
#include <stdint.h>
#include <sys/types.h>
#include "../at.h"
#include "at_agent.h"

fsinf_t* del_fs(fsinf_t* fs, char* mountpoint)
{
    fsinf_t *first = fs;
    fsinf_t *prev = fs;

    while( fs != NULL) {

        if( strcmp(mountpoint, fs->mountpoint) == 0 ) {
            if(fs->dev != NULL) free(fs->dev);
            if(fs->mountpoint != NULL) free(fs->mountpoint);
            if(fs->fstype != NULL) free(fs->fstype);

            LIST_UNLINK(fs);

            break;
        }
        prev = fs;
        fs = fs->next;
    }
    return first;
}

#ifdef DEBUG
void dump_fs(fsinf_t* fs)
{
    printf("FS Info:\n");
    while(fs != NULL) {
        printf("  Mountpoint %s\n", fs->mountpoint);
        printf("   Device     : %s\n", fs->dev);
        printf("   FS Type    : %s\n", fs->fstype);
        printf("   Blocks     : %lu\n", fs->blks_total);
        printf("   Blks free  : %lu\n", fs->blks_free);
        printf("   Blks avail : %lu\n", fs->blks_avail);
        printf("   Blk size   : %lu\n", fs->block_size);
        printf("   Inodes     : %lu\n", fs->inodes_ttl);
        printf("   Inds free  : %lu\n", fs->inodes_free);
        fs = fs->next;
    }
}
#endif

void poll_fs(fsinf_t* fs)
{
    struct statfs sb;
    while(fs != NULL) {
        if( statfs(fs->mountpoint, &sb) == 0 ) {
            fs->blks_total = sb.f_blocks;
            fs->blks_free = sb.f_bfree;
            fs->blks_avail = sb.f_bavail;
            fs->block_size = sb.f_bsize;
            fs->inodes_ttl = sb.f_files;
            fs->inodes_free = sb.f_ffree;
            sprintf(fs->fstype = malloc(20), "0x%lx", sb.f_type);
        }
        fs = fs->next;
    }
}

fsinf_t* push_fs(fsinf_t* fs, char *mountpoint, char *device)
{
    struct statfs sb;
    fsinf_t *first = fs;

    /* For an empty set */
    if (fs == NULL) {
        fs = malloc(sizeof(fsinf_t));
        memset(fs, 0, sizeof(fsinf_t));
        first = fs;
    } else {
        while( fs->next != NULL) {
            if( strcmp(device, fs->dev) == 0 ) return first;
            fs = fs->next;
        }
        fs->next = malloc(sizeof(fsinf_t));
        memset(fs->next, 0, sizeof(fsinf_t));
        fs = fs->next;
    }

    if ( statfs(mountpoint, &sb) == 0 ) {
        fs->mountpoint = strdup(mountpoint);
        fs->dev = strdup(device);
        fs->next = NULL;
    }

    return first;
}

fsinf_t* init_fs(fsinf_t* fs)
{
    FILE *fh;
    int rc = 0;

    while(fs != NULL) fs = del_fs(fs, fs->mountpoint);

    char str1[LINE_BUFFER_SZ];
    char str2[STR_BUFFER_SZ];

    fh = fopen("/etc/mtab", "r");
    while((rc = fscanf(fh, "%s %s %*s %*s %*s %*u %*u", str1, str2)) != EOF) {
        if(strncmp(str1, "/dev/", 5) == 0) {
            fs = push_fs(fs, str2, str1);
        } else if ( (strncmp(str1, "tmpfs", 4) == 0) &&
                    (strncmp(str2, "/tmp", 4) == 0) ) {
            fs = push_fs(fs, str2, str1);
        }
    }
    fclose(fh);

    return fs;
}
