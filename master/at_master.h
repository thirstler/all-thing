#ifndef AT_MASTER_H_
#define AT_MASTER_H_

#define DEFAULT_MASTER_MON_RATE 5000000
#define MASTER_PID_FILE "/var/run/at_master.pid"
#define DEFAULT_SERVER_PORT "4567"

#define DEFAULT_DB_DB "webthing"
#define DEFAULT_DB_APP "webthing"
#define DEFAULT_DB_USER "webthing"
#define DEFAULT_DB_PW ""
#define DEFAULT_DB_HOST "localhost"
#define DEFAULT_DB_PORT "5432"
#define RETRY_DB_CONN_SEC 10

/*
 * Default root-level keys to try and calculate data rates in.
 */
#define CALC_RATE_KEYS {"cpus", "cpu_ttls", "iodev", "iface"}

/* packet assembly buffer errors */
#define ASMBL_BUFFER_COMPLETE   0x00000000
#define ASMBL_BUFFER_SUCCESS    0x00000000
#define ASMBL_BUFFER_FOUND      0x00000000
#define ASMBL_BUFFER_INCOMPLETE 0x00000001
#define ASMBL_BUFFER_NOT_FOUND  0x00000002
#define ASMBL_BUFFER_BAD_MSG    0x00000003

/* operation types for database operations queue */
#define DB_AGENT_CACHE_UPDATE_QUEUE 0
#define DB_AGENT_TS_UPDATE_QUEUE 1

/* database operations queue stuff */
#define DB_QUEUE_SLEEP 10000000 /* in nanoseconds */
#define MAX_DB_QUEUE_LEN 16384
#define DEFAULT_DB_COMMIT_RATE 300

/* default max number of trackable objects */
#define DATA_STRUCT_AR_SIZE 65536

#define ASMBL_BUF_PRUNE_COUNT 500
#define MASTER_ASMBL_BUF_AGE 15

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

#define AT_MASTER_HELP "\n\
Use: at_master [OPTION]\n\
Start the all-thing master daemon.\n\
\n\
Arguments:\n\
  -h, --help            this help\n\
  -D, --nofork          do not fork into a daemon (useful for debugging)\n\
  -p, --port=PORT       change node-listening port\n\
  -s, --server=PORT     change port for data service\n\
  -r, --monitor=RATE    change monitoring rate\n\
  -c, --config=FILE     get configuration from specified file\n\
  -d, --debug=LEVEL     specify a debug level (0-7)\n\
\n\
"

typedef struct master_config_s {
    useconds_t mon_rate;
    char *listen_addr;
    char listen_port[7];
    char *server_addr;
    char server_port[7];
    char config_file[255];
    char runuser[255];
    char daemon;
    int rprt_hndlrs;
    int log_level;
    time_t def_commit_rate;

    /* database info cleared after connection established */
    char *db_host;
    char *db_port;
    char *db_db;
    char *db_user;
    char *db_pw;

} master_config_t;

typedef struct json_str_assembly_bfr_s {
    uint64_t id;
    struct timeval ts;
    size_t charcount;
    uint64_t msgid;
    char *json_str;
    size_t strlen;
    void *next;
} json_str_assembly_bfr_t;

typedef struct rprt_hdr_s {
    uint64_t hostid;
    uint64_t seq;
    uint64_t msg_sz;
    uint64_t offset;
    uint64_t msgid;
    struct sockaddr fromaddr;
} rprt_hdr_t;

typedef struct obj_rec_s {
	uint64_t id;
	time_t commit_rate;
	time_t last_commit;
	json_t *record;
} obj_rec_t;

typedef struct data_op {
	int type;
	void *record;
} data_op_t;

typedef struct master_global_data_s {
	obj_rec_t **obj_rec;
    size_t obj_rec_sz;
    data_op_t data_ops_queue[16384];
} master_global_data_t;


/******************************************************************************
 * Report and server listener threads ****************************************/
void *report_listener(void *dptr);
void *server_listener(void *dptr);

/**
 * Fetch record with ID "id" from the master system data object
 */
obj_rec_t* get_obj_rec(uint64_t id, master_global_data_t *data);

/**
 * Add obj_rec_t pointer to the master data object. Does not copy data so
 * don't free source - it lives here how.
 */
int add_obj_rec(master_global_data_t *master, obj_rec_t *newobj);
/**
 * Remove data object "id" from the master system data object
 */
int rm_obj_rec(uint64_t id, master_global_data_t *data);

/**
 * Free a data object. Simply calls json_decref() and frees the struct. May
 * do more later.
 */
void free_obj_rec(obj_rec_t *dobj);

/**
 * Calculate rates from selected counters
 */
int calc_rates_linux(json_t *standing, json_t *incomming);

/**
 * Rate calculation
 */
json_t* get_these_rates(
		json_t *standing,
		json_t *incomming,
		const char *fields[],
		size_t num_fields);

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
int drop_agent_cache();

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
