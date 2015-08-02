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
#include "../ini.h"
#include "../at.h"
#include "at_master.h"

char on;
master_config_t *cfg;
PGconn *pgconn;

/* Multiple threads need to write to the db ops queue */
pthread_mutex_t db_queue_ops_mtx;

static int config_handler(
        void* user,
        const char* section,
        const char* name,
        const char* value)
{
    master_config_t* cfg = (master_config_t*)user;

    if(strcmp(section, "global") == 0) {
        if(strcmp(name, "master_target_port") == 0)
            strcpy(cfg->listen_port, value);
        if(strcmp(name, "listen_port") == 0)
            strcpy(cfg->listen_port, value);
        if(strcmp(name, "master_data_port") == 0)
            strcpy(cfg->server_port, value);
    }

    if(strcmp(section, "master") == 0) {

        if(strcmp(name, "commit_to_db") == 0)
            cfg->def_commit_rate = atoi(value);
    	if(strcmp(name, "report_listen_addr") == 0)
    		cfg->listen_addr = strdup(value);
    	if(strcmp(name, "server_listen_addr") == 0)
    	    cfg->server_addr = strdup(value);
        if(strcmp(name, "mon_loop_rate") == 0)
            cfg->mon_rate = atoi(value);
        if(strcmp(name, "runuser") == 0){
            strcpy(cfg->runuser, value);
        }
        if(strcmp(name, "max_report_handlers") == 0)
            cfg->rprt_hndlrs = atoi(value);
        if(strcmp(name, "log_level") == 0) {
            cfg->log_level = atoi(value);
            if(cfg->log_level < 0 || cfg->log_level > 7) {
            	cfg->log_level = DEFAULT_LOG_LEVEL;
            }
        }
        if(strcmp(name, "default_commit_rate") == 0)
            cfg->rprt_hndlrs = atoi(value);
    }

    /* This info is only held until the db connection is established */
    if(strcmp(section, "database") == 0) {
		if(strcmp(name, "hostname") == 0)   cfg->db_host = strdup(value);
		if(strcmp(name, "port") == 0)       cfg->db_port = strdup(value);
		if(strcmp(name, "username") == 0)   cfg->db_user = strdup(value);
		if(strcmp(name, "password") == 0)   cfg->db_pw = strdup(value);
		if(strcmp(name, "database") == 0)   cfg->db_db = strdup(value);
	}
    return 1;
}

/*
 * Set default values defined in at.h
 */
static void set_cfg_defaults(master_config_t* cfg)
{
    cfg->mon_rate = DEFAULT_MASTER_MON_RATE;
    cfg->def_commit_rate = DEFAULT_DB_COMMIT_RATE;
    strcpy(cfg->runuser, DEFAULT_RUNUSER);
    strcpy(cfg->config_file, DEFAULT_CONFIG);
    strcpy(cfg->server_port, DEFAULT_SERVER_PORT);
    strcpy(cfg->listen_port, DEFAULT_REPORT_PORT);
    cfg->daemon = 1;
    cfg->log_level = DEFAULT_LOG_LEVEL;
}

static void reinit(int sig)
{
	/* not implemented */
}

static void quitit(int sig)
{
    printf("exit requested\n");
    on = 0;
    sleep(1);
    /* break all the recv() functions */
    signal(SIGINT, SIG_IGN);
}

static void clear_config()
{
	if(cfg->listen_addr != NULL) free(cfg->listen_addr);
	if(cfg->server_addr != NULL) free(cfg->server_addr);
	if(cfg->db_db != NULL) free(cfg->db_db);
	if(cfg->db_host != NULL) free(cfg->db_host);
	if(cfg->db_port != NULL) free(cfg->db_port);
    free(cfg);
}


static void clear_data_master(master_global_data_t *data_master)
{
	size_t i = 0;

	if(data_master == NULL) return;
	for(i = 0; i < data_master->obj_rec_sz; i += 1) {
		free_obj_rec(data_master->obj_rec[i]);
	}
	if(data_master->obj_rec != NULL) free(data_master->obj_rec);
	free(data_master);
}

int main(int argc, char *argv[])
{
    on = 1;
    unsigned int mon_count = 0;
    int rc;
    struct passwd *mememe;
    pthread_t node_listen;
    pthread_t data_server;
    pthread_t database_ops;
    int db_retry_sec = RETRY_DB_CONN_SEC;
    time_t loop_time, last_db_con_atempt = 0;
    data_server_in_t data_server_in;
    listener_in_t listener_in;

    master_global_data_t *data_master;
    openlog("at_master", LOG_CONS|LOG_PERROR, LOG_USER);
    FILE *pidf;
    pid_t mypid;

    /* Allocate config object and set defaults */
    cfg = malloc(sizeof(master_config_t));
    memset(cfg, 0, sizeof(master_config_t));
    set_cfg_defaults(cfg);

    /* Now overwrite with config file */
    if (ini_parse(cfg->config_file, config_handler, cfg) < 0) {
        syslog(LOG_ERR, "Can't load '%s'\n", cfg->config_file);
    }

    /* See what the command line has to say.... */
    while ((rc = getopt (argc, argv, "hDp:r:c:d:")) != -1) {
        switch(rc) {
        case 'h':
            printf("%s", AT_MASTER_HELP);
            return EXIT_SUCCESS;
            break;
        case 'D':
            cfg->daemon = 0;
            break;
        case 'd':
            cfg->log_level = atoi(optarg);
            if(cfg->log_level < 0 || cfg->log_level > 7) {
            	fprintf(stderr,
            			"log-level makes no sense, setting to LOG_WARN\n");
            	cfg->log_level = 4;
            }
            break;
        case 'p':
            strcpy(cfg->listen_port, optarg);
            break;
        case 'r':
            cfg->mon_rate = atoi(optarg)*1000000;
            break;
        case 'c':
            strcpy(cfg->config_file, optarg);
            /* Ok, we're doing this again (order is important!) */
            if (ini_parse(cfg->config_file, config_handler, cfg) < 0) {
                syslog(LOG_ERR, "Can't load '%s'", cfg->config_file);
            }
            break;
        case '?':
            syslog(LOG_ERR, "argument expected");
            break;
        default:
            fprintf(stderr, "unexpected flag: %c", rc);
        }
    }

    /* forking */
    if(cfg->daemon) {
        rc = fork();
        if(rc != 0) {
            exit(EXIT_SUCCESS);
        }
        fclose(stderr);
        fclose(stdout);
        openlog("at_master", LOG_CONS, LOG_USER);
    } else {
        openlog("at_master", LOG_CONS|LOG_PERROR, LOG_USER);
    }

    mypid = getpid();
    pidf = fopen(MASTER_PID_FILE, "w");
    fprintf(pidf, "%u", mypid);
    fclose(pidf);

    /* Set user */
    mememe = getpwnam(cfg->runuser);
    if(mememe != NULL) {
        setreuid(mememe->pw_uid, mememe->pw_uid);
        setregid(mememe->pw_gid, mememe->pw_gid);
    } else {
        syslog(LOG_ERR, "running as root (YUCK!), please configure a user");
    }

    /* Set the log mask */
    rc = 0;
    switch(cfg->log_level) {
    case 7: rc |= LOG_MASK(LOG_DEBUG);
    case 6: rc |= LOG_MASK(LOG_INFO);
    case 5: rc |= LOG_MASK(LOG_NOTICE);
    case 4: rc |= LOG_MASK(LOG_WARNING);
    case 3: rc |= LOG_MASK(LOG_ERR);
    case 2: rc |= LOG_MASK(LOG_CRIT);
    case 1: rc |= LOG_MASK(LOG_ALERT);
    case 0: rc |= LOG_MASK(LOG_EMERG);
    }
    setlogmask(rc);

    syslog(LOG_CRIT, "starting up with log level %d (%x)", cfg->log_level, rc);

    /* Handle some signals */
	if (signal(SIGINT, quitit) == SIG_IGN)  signal (SIGINT, SIG_IGN);
    if (signal(SIGTERM, quitit) == SIG_IGN) signal (SIGTERM, SIG_IGN);
    if (signal(SIGHUP, reinit) == SIG_IGN)  signal (SIGHUP, SIG_IGN);

    data_master = malloc(sizeof(master_global_data_t));
    data_master->obj_rec = NULL;
    data_master->obj_rec_sz = 0;

    /* initialize database ops queue */
    memset(data_master->data_ops_queue, '\0', (sizeof(data_op_t)*MAX_DB_QUEUE_LEN));

    /* Connect to database */
    pgconn = db_connect(cfg);

    if(PQstatus(pgconn) != CONNECTION_OK) {
    	syslog(LOG_ERR, "failed to connect to database but I'll keep going...");
    }

    /* Clean-up in case of unclean shutdown */
    drop_agent_cache();

    /* Create agent cache table */
    mk_cache_tbl();

    /* Spin-off threads */

    data_server_in.master = data_master;
    data_server_in.ds_socket = -1;
    listener_in.master = data_master;
    listener_in.lsn_socket = -1;

    rc = pthread_create(
            &node_listen, NULL, report_listener, (void*) &listener_in);
    rc = pthread_create(
            &data_server, NULL, server_listener, (void*) &data_server_in);
    rc = pthread_create(
            &database_ops, NULL, database_ops_queue, (void*) data_master);

    pthread_mutex_init(&db_queue_ops_mtx, NULL);

    /* Main (monitor) loop */
    for(mon_count=0; on; mon_count +=1) {
        usleep(cfg->mon_rate);
        loop_time = time(NULL);

        /* retry database connection if necessary */
        if(PQstatus(pgconn) != CONNECTION_OK && ((loop_time - last_db_con_atempt) > db_retry_sec) )
        {
        	pgconn = db_connect(cfg);
        	if(PQstatus(pgconn) == CONNECTION_OK) {
        		syslog(LOG_INFO, "successfully connected to database");
			} else {
				syslog(LOG_DEBUG, "failed to reconnect to database");
			}
        	last_db_con_atempt = loop_time;
        }
    }

    /* Stop the loops! */
    shutdown(data_server_in.ds_socket, SHUT_RDWR);
    shutdown(listener_in.lsn_socket, SHUT_RDWR);

    /* Clean up db agent cache */
    drop_agent_cache();
    pthread_join(node_listen, NULL);
    pthread_join(data_server, NULL);
    pthread_join(database_ops, NULL);
    pthread_mutex_destroy(&db_queue_ops_mtx);

    PQfinish(pgconn);
    clear_data_master(data_master);
    clear_config();

    syslog(LOG_CRIT, "main thread exited");
    syslog(LOG_CRIT, "stick a fork in me, I'm done");

    return EXIT_SUCCESS;
}
