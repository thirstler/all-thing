#ifndef INITDB_H_
#define INITDB_H_

#define INIT_TMP_SBL_SQL "CREATE TABLE IF NOT EXISTS agent_cache (\
hostid        integer PRIMARY KEY,\
timestamp     timestamp NOT NULL,\
misc          json NULL,\
meta          json NULL,\
sysload       json NULL,\
memory        json NULL,\
cpu_ttl       json NULL,\
cpu_static    json NULL,\
cpu           json NULL,\
iface         json NULL,\
iodev         json NULL,\
fsinf         json NULL\
)"

#define INSERT_TMP_SBL_SQL "INSERT INTO agent_cache (\
hostid,\
timestamp,\
misc,\
meta,\
sysload,\
memory,\
cpu_ttl,\
cpu_static,\
cpu,\
iface,\
iodev,\
fsinf) VALUES (%ul,'%s','%s','%s','%s','%s','%s','%s','%s','%s','%s','%s')"

#define UPDATE_TMP_SBL_SQL "UPDATE agent_cache SET \
timestamp = %s',\
misc = '%s',\
meta = '%s',\
sysload = '%s',\
memory = '%s',\
cpu_ttl = '%s',\
cpu_static = '%s',\
cpu = '%s',\
iface = '%s',\
iodev = '%s',\
fsinf = '%s' WHERE hostid = %ul"

#define INIT_RECORD_TABLE "CREATE TABLE IF NOT EXISTS %lx (\
id            serial primary key,\
timestamp     timestamp NOT NULL,\
misc          json NULL,\
meta          json NULL,\
sysload       json NULL,\
memory        json NULL,\
cpu_ttl       json NULL,\
cpu_static    json NULL,\
cpu           json NULL,\
iface         json NULL,\
iodev         json NULL,\
fsinf         json NULL\
)"

#define INSERT_RECORD_TABLE "INSERT INTO '%lx' (\
timestamp,\
misc,\
meta,\
sysload,\
memory,\
cpu_ttl,\
cpu_static,\
cpu,\
iface,\
iodev,\
fsinf) VALUES ('%s','%s','%s','%s','%s','%s','%s','%s','%s','%s','%s')"

#define UPDATE_RECORD_TABLE "UPDATE '%lx' SET \
timestamp = %s',\
misc = '%s',\
meta = '%s',\
sysload = '%s',\
memory = '%s',\
cpu_ttl = '%s',\
cpu_static = '%s',\
cpu = '%s',\
iface = '%s',\
iodev = '%s',\
fsinf = '%s' WHERE id = %u"

/*
 * Build connection string and connect (eats values from cfg)
 */
PGconn *db_connect(master_config_t *cfg);

/*
 * Make cache table for real-time user
 */
int mk_cache_tbl(PGconn *conn);

/*
 * Record snapshots of all records
 */
int snap_rec_tbls(master_global_data_t *data_master, PGconn *conn);


#endif
