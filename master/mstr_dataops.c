/*
 * File: mstr_dataops.c
 * Desc: Calculators and functions for managing the master data structure
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
#include <syslog.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <jansson.h>
#include <libpq-fe.h>
#include <time.h>
#include <uuid/uuid.h>
#include "../at.h"
#include "at_master.h"

extern master_config_t *cfg;

void free_obj_rec(obj_rec_t *dobj)
{
    if(dobj == NULL) return;
    json_object_clear(dobj->record);
    json_decref(dobj->record);
    free(dobj);
}

int rm_obj_rec(uuid_t uuid, master_global_data_t *data)
{
    size_t i;

    for(i = 0; i < data->obj_rec_sz; i += 1) {
        if( uuid_compare(uuid, data->obj_rec[i]->uuid) == 0) {
            free_obj_rec(data->obj_rec[i]);
            data->obj_rec_sz -= 1;
            for(;i < data->obj_rec_sz; i += 1)
                data->obj_rec[i] = data->obj_rec[i+1];
            return EXIT_SUCCESS;
        }
    }
    return EXIT_SUCCESS;
}

obj_rec_t* get_obj_rec(uuid_t uuid, master_global_data_t *data)
{
    static char idstr[40]; 
    
    // Brute force for now, I'll get fancy later
    size_t i;
    for(i=0; i < data->obj_rec_sz; i += 1) {
        if( uuid_compare(uuid, data->obj_rec[i]->uuid) == 0 ) {
            syslog(LOG_DEBUG,
                    "get_data_obj() found something at position %lu", i);
            return data->obj_rec[i];
        }
    }
    uuid_unparse(uuid, idstr);
    syslog(LOG_DEBUG, "no object with id %s in store", idstr);
    return NULL;
}

/* Just shove it on the end, get fancy laster */
int add_obj_rec(master_global_data_t *master, obj_rec_t *newobj)
{
    size_t i;

    if( uuid_is_null(newobj->uuid) ) return EXIT_FAILURE;

    if( master->obj_rec_sz >= DATA_STRUCT_AR_SIZE) {
        syslog(LOG_ERR, "data buffer full, increase to > %d",
                DATA_STRUCT_AR_SIZE);
        return EXIT_FAILURE;
    }

    for(i=0; i < master->obj_rec_sz; i += 1) {
        if( uuid_compare(newobj->uuid, master->obj_rec[i]->uuid) == 0 )
            return EXIT_FAILURE;
    }

    newobj->commit_rate = cfg->def_commit_rate;
    newobj->last_commit = time(NULL);

    master->obj_rec[master->obj_rec_sz] = newobj;
    master->obj_rec_sz += 1;

    syslog(LOG_DEBUG, "add_new_data_obj(), new size %lu", master->obj_rec_sz);

    return EXIT_SUCCESS;
}

#define RATE_CALC(key, curobj, newobj, target, tdiff) {\
    json_object_set_new(\
        target,\
        key,\
        json_real(\
            (float)(json_integer_value(json_object_get(newobj, key)) -\
            json_integer_value(json_object_get(curobj, key)))/\
            ((float) tdiff.tv_sec +\
            ((float) tdiff.tv_usec/1000000))));\
}

json_t* get_rates(json_t *old, json_t *new, struct timeval *t_diff)
{
    json_t *new_obj = NULL, *value, *jptr;
    const char *key;
    const char *rptr;
    size_t index;
    int o_type = json_typeof(old);
    int n_type = json_typeof(new);

    if(o_type != n_type) return NULL;

    switch(o_type) {
    case(JSON_OBJECT):

        new_obj = json_object();
        json_object_foreach(old, key, value) {
            if(strncmp("c|", key, 2) != 0) continue;
            rptr = key+2; // Get the label w/out the counter tag
            jptr = json_object_get(new, key);
            if(jptr == NULL) continue;
            json_object_set_new(new_obj, rptr, get_rates(value, jptr, t_diff));
        }
        break;

    case(JSON_ARRAY):

#if JANSSON_VERSION_HEX < 0x020500
#define json_array_foreach(array, index, value) \
        for(index = 0; \
                index < json_array_size(array) && (value = json_array_get(array, index)); \
                index++)
#endif
        /* Arrays have no ability to indicated data type per item. We have to
         * assume they are all counters. */
        new_obj = json_array();
        json_array_foreach(old, index, value) {
            jptr = json_array_get(new, index);
            if(jptr == NULL) continue;
            json_array_append_new(new_obj, get_rates(value, jptr, t_diff));
        }
        break;

    case(JSON_INTEGER):

        /* Return rate per second as a double */
        new_obj = json_real(
                (double)(json_integer_value(new) - json_integer_value(old)) /
                ((double)t_diff->tv_sec + ((double)t_diff->tv_usec/1000000))
            );
        break;

    case(JSON_REAL):

        /* Not sure why this would happen, but here it is...
         * Return rate per second as a double */
        new_obj = json_real(
                (json_real_value(new) - json_real_value(old)) /
                ((double)t_diff->tv_sec + ((double)t_diff->tv_usec/1000000))
            );
        break;

    default: new_obj = NULL;
    }

    return new_obj;
}

json_t* get_these_rates(
        json_t *standing,
        json_t *incomming,
        const char *fields[],
        size_t num_fields)
{
    struct timeval diff, standing_ts, incomming_ts;
    json_t *ts;
    json_t *target = json_object();
    size_t i;

    if ((ts = json_object_get(standing, "ts")) == NULL) return NULL;
    standing_ts.tv_sec = json_integer_value(json_object_get(ts, "tv_sec"));
    standing_ts.tv_usec = json_integer_value(json_object_get(ts, "tv_usec"));

    if ((ts = json_object_get(incomming, "ts")) == NULL) return NULL;
    incomming_ts.tv_sec = json_integer_value(json_object_get(ts, "tv_sec"));
    incomming_ts.tv_usec = json_integer_value(json_object_get(ts, "tv_usec"));

    timersub(&incomming_ts, &standing_ts, &diff);

    for(i = 0; i < num_fields; i += 1) {
        json_object_set_new(target,
                fields[i],
                get_rates(
                        json_object_get(standing, fields[i]),
                        json_object_get(incomming, fields[i]),
                        &diff
                ));
    }

    return target;
}
