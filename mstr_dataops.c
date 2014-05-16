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

int calc_data_rates(json_t *standing, json_t *incomming)
{
	struct timeval diff, standing_ts, incomming_ts;
	json_t *ts;
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
		json_object_set_new(tmp_st, "__rates", tmp_calc = json_object());
	RATE_CALC("user", tmp_st, tmp_in, tmp_calc, diff);
	RATE_CALC("nice", tmp_st, tmp_in, tmp_calc, diff);
	RATE_CALC("system", tmp_st, tmp_in, tmp_calc, diff);
	RATE_CALC("idle", tmp_st, tmp_in, tmp_calc, diff);
	RATE_CALC("iowait", tmp_st, tmp_in, tmp_calc, diff);
	RATE_CALC("irq", tmp_st, tmp_in, tmp_calc, diff);
	RATE_CALC("s_irq", tmp_st, tmp_in, tmp_calc, diff);
	RATE_CALC("steal", tmp_st, tmp_in, tmp_calc, diff);
	RATE_CALC("guest", tmp_st, tmp_in, tmp_calc, diff);
	RATE_CALC("n_guest", tmp_st, tmp_in, tmp_calc, diff);
	RATE_CALC("intrps", tmp_st, tmp_in, tmp_calc, diff);
	RATE_CALC("s_intrps", tmp_st, tmp_in, tmp_calc, diff);
	RATE_CALC("ctxt", tmp_st, tmp_in, tmp_calc, diff);

	/* Set rates for individual CPUS *****************************************/
	tmp_arr_in = json_object_get(incomming, "cpus");
	tmp_arr_st = json_object_get(standing, "cpus");

	for(i=0; i < json_array_size(tmp_arr_in); i += 1) {
		tmp_in = json_array_get(tmp_arr_in, i);
		tmp_st = json_array_get(tmp_arr_st, i);
		tmp_calc = json_object_get(tmp_st, "__rates");
		if(tmp_calc == NULL)
			json_object_set_new(tmp_st, "__rates", tmp_calc = json_object());

		RATE_CALC("user", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("nice", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("system", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("idle", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("iowait", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("irq", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("s_irq", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("steal", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("guest", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("n_guest", tmp_st, tmp_in, tmp_calc, diff);
	}

	/* Set rates for network counters ****************************************/
	tmp_arr_in = json_object_get(incomming, "iface");
	tmp_arr_st = json_object_get(standing, "iface");
	for(i=0; i < json_array_size(tmp_arr_in); i += 1) {
		tmp_in = json_array_get(tmp_arr_in, i);
		tmp_st = json_array_get(tmp_arr_st, i);
		tmp_calc = json_object_get(tmp_st, "__rates");
		if(tmp_calc == NULL)
			json_object_set_new(tmp_st, "__rates", tmp_calc = json_object());
		RATE_CALC("rx_packets", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("rx_bytes", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("rx_errs", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("rx_drop", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("rx_fifo", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("rx_frame", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("rx_comp", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("rx_multi", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("tx_packets", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("tx_bytes", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("tx_errs", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("tx_drop", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("tx_fifo", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("tx_colls", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("tx_carr", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("tx_comp", tmp_st, tmp_in, tmp_calc, diff);
	}

	/* Set rates for I/O devices counters ************************************/
	tmp_arr_in = json_object_get(incomming, "iodev");
	tmp_arr_st = json_object_get(standing, "iodev");
	for(i=0; i < json_array_size(tmp_arr_in); i += 1) {
		tmp_in = json_array_get(tmp_arr_in, i);
		tmp_st = json_array_get(tmp_arr_st, i);
		tmp_calc = json_object_get(tmp_st, "__rates");
		if(tmp_calc == NULL)
			json_object_set_new(tmp_st, "__rates", tmp_calc = json_object());
		RATE_CALC("reads", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("read_sectors", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("reads_merged", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("msec_reading", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("writes", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("write_sectors", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("writes_merged", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("msec_writing", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("msec_ios", tmp_st, tmp_in, tmp_calc, diff);
		RATE_CALC("weighted_ios", tmp_st, tmp_in, tmp_calc, diff);
	}

	return EXIT_SUCCESS;
}
