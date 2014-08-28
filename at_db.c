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
#include "at.h"
#include "at_db.h"

PGconn *db_connect(master_config_t *cfg)
{
	PGconn *pgconn;
	const char **keywords;
	const char **values;
	size_t as = sizeof(char**) * 8;
	keywords = malloc(as);
	values = malloc(as);

	memset(keywords, '\0', as);
	memset(values, '\0', as);

	keywords[0] = strdup("host");
	values[0] = (cfg->db_host == NULL) ? strdup("localhost") : cfg->db_host;

	keywords[1] = strdup("port");
	values[1] = (cfg->db_port == NULL) ? strdup("5432") : cfg->db_port;

	keywords[2] = strdup("dbname");
	values[2] = (cfg->db_db == NULL) ? strdup("webthing") : cfg->db_db;

	keywords[3] = strdup("user");
	values[3] = (cfg->db_user == NULL) ? strdup("webthing") : cfg->db_user;

	keywords[4] = strdup("password");
	values[4] = (cfg->db_pw == NULL) ? strdup("") : cfg->db_pw;

	keywords[5] = strdup("application_name");
	values[5] = strdup("webthing");

	pgconn = PQconnectdbParams(keywords, values, 0);

	/* Destroy credentials */
	memset(cfg->db_user, '\0', strlen(cfg->db_user));
	free(cfg->db_user);
	memset(cfg->db_pw, '\0', strlen(cfg->db_pw));
	free(cfg->db_pw);

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

int mk_cache_tbl(PGconn *conn)
{
	PGresult *res;
	res = PQexec(conn, INIT_TMP_SBL_SQL);
	syslog(LOG_DEBUG, "mk tmp table status: %s", PQcmdStatus(res));
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

int init_cache_entry(sysinf_t *newme, PGconn *conn)
{
	static char q[65536];
}

int snap_rec_tbls(master_global_data_t *data_master, PGconn *conn)
{
	static char q[65536];
	size_t i;
	char *sysload, *memory, *cpu_ttls, *cpu_static, *cpus, *iface, *iodev, *fsinf, *meta, *ut, *it;

	for(i=0; i < data_master->obj_rec_sz; i += 1) {
		sprintf(q, INIT_RECORD_TABLE, data_master->obj_rec[i]->id);
		PQexec(conn, q);

		meta = json_dumps(json_object_get(
				data_master->obj_rec[i]->record,
				"meta"), JSON_COMPACT|JSON_ENSURE_ASCII);

		sysload = json_dumps(json_object_get(
				data_master->obj_rec[i]->record,
				"sysload"), JSON_COMPACT|JSON_ENSURE_ASCII);

		memory = json_dumps(json_object_get(
				data_master->obj_rec[i]->record,
				"memory"), JSON_COMPACT|JSON_ENSURE_ASCII);

		cpu_ttls = json_dumps(json_object_get(
				data_master->obj_rec[i]->record,
				"cpu_ttls"), JSON_COMPACT|JSON_ENSURE_ASCII);

		cpu_static = json_dumps(json_object_get(
				data_master->obj_rec[i]->record,
				"cpu_static"), JSON_COMPACT|JSON_ENSURE_ASCII);

		cpus = json_dumps(json_object_get(
				data_master->obj_rec[i]->record,
				"cpus"), JSON_COMPACT|JSON_ENSURE_ASCII);

		iface = json_dumps(json_object_get(
				data_master->obj_rec[i]->record,
				"iface"), JSON_COMPACT|JSON_ENSURE_ASCII);

		iodev = json_dumps(json_object_get(
				data_master->obj_rec[i]->record,
				"iodev"), JSON_COMPACT|JSON_ENSURE_ASCII);

		fsinf = json_dumps(json_object_get(
				data_master->obj_rec[i]->record,
				"fs"), JSON_COMPACT|JSON_ENSURE_ASCII);

		ut = json_dumps(json_object_get(
				data_master->obj_rec[i]->record,
				"uptime"), JSON_COMPACT|JSON_ENSURE_ASCII);

		it = json_dumps(json_object_get(
				data_master->obj_rec[i]->record,
				"idletime"), JSON_COMPACT|JSON_ENSURE_ASCII);

	}

}
