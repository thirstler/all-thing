/*
 * at_db.c
 *
 *  Created on: Jul 25, 2014
 *      Author: jrussler
 */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <pwd.h>
#include <syslog.h>
#include <pthread.h>
#include <netinet/in.h>
#include <libpq-fe.h>
#include <jansson.h>
#include "../at.h"
#include "at_master.h"

extern PGconn *pgconn;
extern char on;

PGconn *db_connect(master_config_t *cfg)
{
	PGconn *pgconn;
	const char **keywords;
	const char **values;
	size_t as = sizeof(char**) * 8;
	keywords = malloc(as);
	values = malloc(as);
	int i;

	char *default_db_host = strdup(DEFAULT_DB_HOST);
	char *default_db_port = strdup(DEFAULT_DB_PORT);
	char *default_db_db = strdup(DEFAULT_DB_DB);
	char *default_db_user = strdup(DEFAULT_DB_USER);
	char *default_db_pw = strdup(DEFAULT_DB_PW);
	char *default_db_app = strdup(DEFAULT_DB_APP);

	memset(keywords, '\0', as);
	memset(values, '\0', as);

	keywords[0] = strdup("host");
	values[0] = (cfg->db_host == NULL) ? default_db_host : cfg->db_host;

	keywords[1] = strdup("port");
	values[1] = (cfg->db_port == NULL) ? default_db_port : cfg->db_port;

	keywords[2] = strdup("dbname");
	values[2] = (cfg->db_db == NULL) ? default_db_db : cfg->db_db;

	keywords[3] = strdup("user");
	values[3] = (cfg->db_user == NULL) ? default_db_user : cfg->db_user;

	keywords[4] = strdup("password");
	values[4] = (cfg->db_pw == NULL) ? default_db_pw : cfg->db_pw;

	keywords[5] = strdup("application_name");
	values[5] = strdup(default_db_app);

	pgconn = PQconnectdbParams(keywords, values, 0);

	free(default_db_host);
	free(default_db_port);
	free(default_db_db);
	free(default_db_user);
	free(default_db_pw);
	free(default_db_app);

	/* Did we connect? */
	if (PQstatus(pgconn) != CONNECTION_OK) {
		syslog(LOG_ERR, "db connection failed: %s",
				PQerrorMessage(pgconn));
		return NULL;
	} else {
		syslog(LOG_INFO, "db connection success");
	}

	return pgconn;
}

inline int drop_agent_cache()
{
	PGresult *res;
	res = PQexec(pgconn, DROP_TMP_SBL_SQL);
	syslog(LOG_DEBUG, "drop agent cache: %s", PQcmdStatus(res));
	PQclear(res);
	return EXIT_SUCCESS;
}

int mk_cache_tbl()
{
	PGresult *res;
	res = PQexec(pgconn, INIT_TMP_SBL_SQL);
	syslog(LOG_DEBUG, "mk tmp table status: %s", PQcmdStatus(res));
	PQclear(res);
	return EXIT_SUCCESS;
}

char *mk_pgts_str_fm_jsontv(json_t *ts)
{
	char *datestr = malloc(32);
	struct timeval cts;
	struct tm *ct;

	cts.tv_sec = json_integer_value(json_object_get(ts, "tv_sec"));
	cts.tv_usec = json_integer_value(json_object_get(ts, "tv_usec"));

	ct = localtime(&cts.tv_sec);

	strftime(datestr, sizeof(datestr), "Y-m-d H:M:S", ct);
	sprintf(datestr, "%s.%lu", datestr, cts.tv_usec);

	return datestr;
}


#define MAX_QUERY_LEN 65536
void write_agent_to_cache(obj_rec_t* data, int new)
{
	/*
	 * Allocate a beefy buffer for what are sure to be some pretty big queries
	 */
	char *query = malloc(sizeof(char)*MAX_QUERY_LEN);
	char *misc, *sysload, *memory, *cpu_ttls, *cpus_static, *cpus, *iface, *iodev, *fs, *meta;
	char *nullobj = strdup("{}");
	const time_t nownow = time(NULL);
	int jsonopts = JSON_COMPACT|JSON_ENSURE_ASCII;
	struct timeval ts;
	json_t *jts;
	double intvl_cnt = 0;
	PGresult *qr;
	ExecStatusType pqrc;

	if (new > 0) {
		snprintf(query, MAX_QUERY_LEN, INSERT_TMP_SBL_SQL,
				(int64_t)data->id, "12/31/1969", 0, 0,
				"{}","{}","{}","{}","{}","{}","{}","{}","{}","{}");
		qr = PQexec(pgconn, query);
		if(PQresultStatus(qr) == PGRES_COMMAND_OK) {
			syslog(LOG_INFO, "created cache entry for %lx", data->id);
		}
		PQclear(qr);
	}

	jts = json_object_get(data->record, "ts");
	if(jts != NULL) {
		ts.tv_sec = json_integer_value(json_object_get(jts, "tv_sec"));
		ts.tv_usec = json_integer_value(json_object_get(jts, "tv_usec"));
	} else {
		ts.tv_sec = 0;
		ts.tv_usec = 0;
	}

	misc = json_dumps(json_object_get(data->record, "misc"), jsonopts);
	if(misc == NULL) misc = nullobj;

	meta = json_dumps(json_object_get(data->record, "meta"), jsonopts);
	if(meta == NULL) meta = nullobj;

	sysload = json_dumps(json_object_get(data->record, "sysload"), jsonopts);
	if(sysload == NULL) sysload = nullobj;

	memory = json_dumps(json_object_get(data->record, "memory"), jsonopts);
	if(memory == NULL) memory = nullobj;

	cpu_ttls = json_dumps(json_object_get(data->record, "cpu_ttls"), jsonopts);
	if(cpu_ttls == NULL) cpu_ttls = nullobj;

	cpus_static = json_dumps(json_object_get(data->record, "cpus_static"), jsonopts);
	if(cpus_static == NULL) cpus_static = nullobj;

	cpus = json_dumps(json_object_get(data->record, "cpus"), jsonopts);
	if(cpus == NULL) cpus = nullobj;

	iface = json_dumps(json_object_get(data->record, "iface"), jsonopts);
	if(iface == NULL) iface = nullobj;

	iodev = json_dumps(json_object_get(data->record, "iodev"), jsonopts);
	if(iodev == NULL) iodev = nullobj;

	fs = json_dumps(json_object_get(data->record, "fs"), jsonopts);
	if(fs == NULL) fs = nullobj;

	snprintf(query, MAX_QUERY_LEN, UPDATE_TMP_SBL_SQL,
			ctime(&nownow),
			ts.tv_sec,
			ts.tv_usec,
			misc,
			meta,
			sysload,
			memory,
			cpu_ttls,
			cpus_static,
			cpus,
			iface,
			iodev,
			fs,
			data->id);

	if( (pqrc = PQresultStatus(qr = PQexec(pgconn, query))) == PGRES_COMMAND_OK) {
		syslog(LOG_DEBUG, "good query: %s", query);
	} else {
		syslog(LOG_WARNING, "failed query: %.72s...", query);
	}

	PQclear(qr);
	free(query);
	if(misc != nullobj) free(misc);
	if(sysload != nullobj) free(sysload);
	if(memory != nullobj) free(memory);
	if(cpu_ttls != nullobj) free(cpu_ttls);
	if(cpus_static != nullobj) free(cpus_static);
	if(cpus != nullobj) free(cpus);
	if(iface != nullobj) free(iface);
	if(iodev != nullobj) free(iodev);
	if(fs != nullobj) free(fs);
	if(meta != nullobj) free(meta);
	free(nullobj);
}

void init_record_tbl(obj_rec_t* data)
{
	char q[1024];
	sprintf(q, INIT_RECORD_TABLE, data->id);
	PQclear(PQexec(pgconn, q));

}

int snap_rec_tbl(obj_rec_t *obj_rec)
{
	char *query = malloc(sizeof(char)*MAX_QUERY_LEN);
	size_t i;
	char *nullobj = strdup("{}");
	char *sysload, *memory, *cpu_ttls, *cpus_static, *cpus, *iface, *iodev, *fsinf, *meta, *misc;
	const time_t nownow = time(NULL);
	json_t *jts;
	struct timeval ts;
	ExecStatusType pqrc;
	PGresult *qr;


	jts = json_object_get(obj_rec->record, "ts");
	if(jts != NULL) {
		ts.tv_sec = json_integer_value(json_object_get(jts, "tv_sec"));
		ts.tv_usec = json_integer_value(json_object_get(jts, "tv_usec"));
	} else {
		ts.tv_sec = 0;
		ts.tv_usec = 0;
	}


	sprintf(query, INIT_RECORD_TABLE, obj_rec->id);
	PQclear(PQexec(pgconn, query));

	misc = json_dumps(json_object_get(
			obj_rec->record,
			"misc"), JSON_COMPACT|JSON_ENSURE_ASCII);
	if(misc == NULL) misc = nullobj;

	meta = json_dumps(json_object_get(
			obj_rec->record,
			"meta"), JSON_COMPACT|JSON_ENSURE_ASCII);
	if(meta == NULL) meta = nullobj;

	sysload = json_dumps(json_object_get(
			obj_rec->record,
			"sysload"), JSON_COMPACT|JSON_ENSURE_ASCII);
	if(sysload == NULL) sysload = nullobj;

	memory = json_dumps(json_object_get(
			obj_rec->record,
			"memory"), JSON_COMPACT|JSON_ENSURE_ASCII);
	if(memory == NULL) memory = nullobj;

	cpu_ttls = json_dumps(json_object_get(
			obj_rec->record,
			"cpu_ttls"), JSON_COMPACT|JSON_ENSURE_ASCII);
	if(cpu_ttls == NULL) cpu_ttls = nullobj;

	cpus_static = json_dumps(json_object_get(
			obj_rec->record,
			"cpus_static"), JSON_COMPACT|JSON_ENSURE_ASCII);
	if(cpus_static == NULL) cpus_static = nullobj;

	cpus = json_dumps(json_object_get(
			obj_rec->record,
			"cpus"), JSON_COMPACT|JSON_ENSURE_ASCII);
	if(cpus == NULL) cpus = nullobj;

	iface = json_dumps(json_object_get(
			obj_rec->record,
			"iface"), JSON_COMPACT|JSON_ENSURE_ASCII);
	if(iface == NULL) iface = nullobj;

	iodev = json_dumps(json_object_get(
			obj_rec->record,
			"iodev"), JSON_COMPACT|JSON_ENSURE_ASCII);
	if(iodev == NULL) iodev = nullobj;

	fsinf = json_dumps(json_object_get(
			obj_rec->record,
			"fs"), JSON_COMPACT|JSON_ENSURE_ASCII);
	if(fsinf == NULL) fsinf = nullobj;

	sprintf(query, INSERT_RECORD_TABLE,
			obj_rec->id,
			ctime(&nownow),
			ts.tv_sec,
			ts.tv_usec,
			misc,
			meta,
			sysload,
			memory,
			cpu_ttls,
			cpus_static,
			cpus,
			iface,
			iodev,
			fsinf);

	if( (pqrc = PQresultStatus(qr = PQexec(pgconn, query))) == PGRES_COMMAND_OK) {
		syslog(LOG_DEBUG, "good query: %s", query);
	} else {
		syslog(LOG_WARNING, "failed query: %.72s...", query);
	}

	PQclear(qr);
	free(query);
	if(misc != nullobj) free(misc);
	if(sysload != nullobj) free(sysload);
	if(memory != nullobj) free(memory);
	if(cpu_ttls != nullobj) free(cpu_ttls);
	if(cpus_static != nullobj) free(cpus_static);
	if(cpus != nullobj) free(cpus);
	if(iface != nullobj) free(iface);
	if(iodev != nullobj) free(iodev);
	if(fsinf != nullobj) free(fsinf);
	if(meta != nullobj) free(meta);
	free(nullobj);
}

uint64_t cache_update_to_db_ops_queue(obj_rec_t* data, data_op_t ops_queue[])
{
	uint64_t i = 0;
	for(i = 0; i < MAX_DB_QUEUE_LEN; i+=1) {
		if(ops_queue[i].record == NULL) {
			ops_queue[i].type = DB_AGENT_CACHE_UPDATE_QUEUE;
			ops_queue[i].record = data;
			return i;
		}
	}
	syslog(LOG_CRIT, "database ops queue is blown, get a bigger queue");
	return -1;
}

uint64_t db_commit_to_db_ops_queue(obj_rec_t* data, data_op_t ops_queue[])
{
	uint64_t i = 0;
	for(i = 0; i < MAX_DB_QUEUE_LEN; i+=1) {
		if(ops_queue[i].record == NULL) {
			ops_queue[i].type = DB_AGENT_TS_UPDATE_QUEUE;
			ops_queue[i].record = data;
			return i;
		}
	}
	syslog(LOG_CRIT, "database ops queue is blown, get a bigger queue");
	return -1;
}

void *database_ops_queue(void *dptr)
{
    master_global_data_t *data_master = dptr;

	struct timespec sleepspec;
	sleepspec.tv_sec = 0;
	sleepspec.tv_nsec = DB_QUEUE_SLEEP;
	uint64_t i = 0;
	uint64_t processed = 0;
	short work;

	while(on) {
		work = 0;
		processed = 0;
		for(i = 0; i < MAX_DB_QUEUE_LEN; i+=1) {
			if(data_master->data_ops_queue[i].record != NULL) {

				work = 1;
				processed += 1;

				/* Agent cache updates */
				if(data_master->data_ops_queue[i].type == DB_AGENT_CACHE_UPDATE_QUEUE) {
					write_agent_to_cache( (obj_rec_t*) data_master->data_ops_queue[i].record, 0);
					data_master->data_ops_queue[i].record = NULL;
				}

				/* system snapshot inserts */
				if(data_master->data_ops_queue[i].type == DB_AGENT_TS_UPDATE_QUEUE) {
					snap_rec_tbl( (obj_rec_t*) data_master->data_ops_queue[i].record);
					data_master->data_ops_queue[i].record = NULL;
				}
			}
		}
		if(processed > 0) syslog(LOG_DEBUG,
				"%li record(s) committed this queue cycle", processed);

		/* Keep this updating frequently without getting into a tight loop
		 * or unnecessarily pausing between queue checks */
		if(work == 0) nanosleep(&sleepspec, NULL);

	}

    syslog(LOG_CRIT, "database operations queue thread exited");
    pthread_exit(NULL);
}
