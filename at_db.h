#ifndef INITDB_H_
#define INITDB_H_

#define DEFAULT_DB_DB "webthing"
#define DEFAULT_DB_APP "webthing"
#define DEFAULT_DB_USER "webthing"
#define DEFAULT_DB_PW ""
#define DEFAULT_DB_HOST "localhost"
#define DEFAULT_DB_PORT "5432"

#define RETRY_DB_CONN_SEC 10

#define INIT_TMP_SBL_SQL "CREATE TABLE IF NOT EXISTS agent_cache (\
hostid        bigint PRIMARY KEY,\
timestamp     timestamp NOT NULL,\
intvl_cnt     interval NULL,\
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
intvl_cnt,\
misc,\
meta,\
sysload,\
memory,\
cpu_ttl,\
cpu_static,\
cpu,\
iface,\
iodev,\
fsinf) VALUES (%li,'%s', '%d  second %d microsecond', '%s','%s','%s','%s','%s','%s','%s','%s','%s','%s')"

#define UPDATE_TMP_SBL_SQL "UPDATE agent_cache SET \
timestamp = '%s',\
intvl_cnt = '%d second %d microsecond',\
misc = '%s',\
meta = '%s',\
sysload = '%s',\
memory = '%s',\
cpu_ttl = '%s',\
cpu_static = '%s',\
cpu = '%s',\
iface = '%s',\
iodev = '%s',\
fsinf = '%s' WHERE hostid = %li"

#define DROP_TMP_SBL_SQL "DROP TABLE IF EXISTS agent_cache"

#define INIT_RECORD_TABLE "CREATE TABLE IF NOT EXISTS \"%lx\" (\
id            serial primary key,\
timestamp     timestamp NOT NULL,\
intvl_cnt     interval,\
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

#define INSERT_RECORD_TABLE "INSERT INTO \"%lx\" (\
timestamp,\
intvl_cnt,\
misc,\
meta,\
sysload,\
memory,\
cpu_ttl,\
cpu_static,\
cpu,\
iface,\
iodev,\
fsinf) VALUES ('%s', '%d second %d microsecond', '%s','%s','%s','%s','%s','%s','%s','%s','%s','%s')"

#define UPDATE_RECORD_TABLE "UPDATE '%lx' SET \
timestamp = %s',\
intvl_cnt = '%d second %d microsecond',\
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
int mk_cache_tbl();

/*
 * Record snapshots of all records
 */
int snap_rec_tbls(master_global_data_t *data_master);

/*
 * Update agent cache
 */
void write_agent_to_cache(obj_rec_t* data, int new);

/*
 * Clean-up agent cache table on start-up/shutdown
 */
inline int drop_agent_cache();

/*
 * Create system table if necessary
 */
void init_record_tbl(obj_rec_t* data);

/**
 * Add to the queue for updates to the agent cache. Note that this does not
 * deep-copy the object and then commit. It just adds a link back to the main
 * JSON object. So if the queue backs-up, the database will simply get a copy
 * of what is and not what was.
 */
uint64_t cache_update_to_db_ops_queue(obj_rec_t* data, data_op_t* ops_queue);


/**
 * Queue up a database snapshot update. Note that this does not deep-copy the
 * object and then commit. It just adds a link back to the main JSON object.
 * So if the queue backs-up, the database will simply get a copy of what is
 * and not what was.
 */
uint64_t db_commit_to_db_ops_queue(obj_rec_t* data, data_op_t ops_queue[]);

/*
 * Database operations queue processing thread
 */
void *database_ops_queue(void *dptr);

#endif
