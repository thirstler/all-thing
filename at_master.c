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
#include "ini.h"
#include "at.h"

char on;
master_config_t *cfg;

static int config_handler(
        void* user,
        const char* section,
        const char* name,
        const char* value)
{
    master_config_t* cfg = (master_config_t*)user;

    if(strcmp(section, "global") == 0) {
        if(strcmp(name, "master_target_port") == 0)
            cfg->listen_port = strdup(value);
    }
    if(strcmp(section, "master") == 0) {
        if(strcmp(name, "mon_rate") == 0)
            cfg->mon_rate = atoi(value);
        if(strcmp(name, "listen_port") == 0)
            cfg->listen_port = strdup(value);
        if(strcmp(name, "runuser") == 0)
            cfg->runuser = strdup(value);
        if(strcmp("name", "max_report_handlers") == 0)
            cfg->rprt_hndlrs = atoi(value);
    }

    return 1;
}

/*
 * Set defaults values defined in at.h
 */
static void set_cfg_defaults(master_config_t* cfg)
{
    cfg->mon_rate = DEFAULT_MASTER_MON_RATE;
    cfg->runuser = strdup(DEFAULT_RUNUSER);
    cfg->config_file = strdup(DEFAULT_CONFIG);
    cfg->daemon = 1;
    cfg->log_level = 4;
}

static void quitit(int signal)
{
    printf("exit requested\n");
    on = 0;
}

static void reinit(int signal)
{

}

static void clear_data_master(master_global_data_t *data_master)
{
	size_t i;

	for(i = 0; i < data_master->system_data_sz; i += 1) {
		free_data_obj(data_master->system_data[i]);
	}
	free(data_master->system_data);
	free(data_master);
}

int main(int argc, char *argv[])
{
    on = 1;
    unsigned int mon_count = 0;
    int rc;
    struct passwd *mememe;
    pthread_t node_listen;
    master_global_data_t *data_master;
    openlog("at_master", LOG_CONS|LOG_PERROR, LOG_USER);

    /* Allocate config object and set defaults */
    cfg = malloc(sizeof(master_config_t));
    memset(cfg, 0, sizeof(master_config_t));
    set_cfg_defaults(cfg);

    /* Now overwrite with config file */
    if (ini_parse(cfg->config_file, config_handler, cfg) < 0) {
        syslog(LOG_ERR, "Can't load '%s'\n", cfg->config_file);
    }

    /* See what the command line has to say.... */
    while ((rc = getopt (argc, argv, "hDp:r:c:")) != -1) {
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
            cfg->listen_port = strdup(optarg);
            break;
        case 'r':
            cfg->mon_rate = atoi(optarg)*1000000;
            break;
        case 'c':
            cfg->config_file = strdup(optarg);
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

    /* Set user */
    mememe = getpwnam(cfg->runuser);
    if(mememe != NULL) {
        setreuid(mememe->pw_uid, mememe->pw_uid);
        setregid(mememe->pw_gid, mememe->pw_gid);
    } else {
        syslog(LOG_ERR, "running as root (YUCK!), please configure a user");
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

    /* Set the log mask */
    rc = 0;
    switch(cfg->log_level) {
    case 7: rc |= LOG_DEBUG;
    case 6: rc |= LOG_INFO;
    case 5: rc |= LOG_NOTICE;
    case 4: rc |= LOG_WARNING;
    case 3: rc |= LOG_ERR;
    case 2: rc |= LOG_CRIT;
    case 1: rc |= LOG_ALERT;
    case 0: rc |= LOG_EMERG;
    }
    setlogmask(rc);

    syslog (LOG_INFO, "starting");

        /* Handle some signals */
    if (signal (SIGINT, quitit) == SIG_IGN)
            signal (SIGINT, SIG_IGN);
    if (signal (SIGTERM, quitit) == SIG_IGN)
                signal (SIGTERM, SIG_IGN);
    if (signal (SIGHUP, reinit) == SIG_IGN)
            signal (SIGHUP, SIG_IGN);

    data_master = malloc(sizeof(master_global_data_t));
    data_master->system_data = NULL;

    /* Spin-off node listener thread */
    rc = pthread_create(
            &node_listen, NULL, report_listener, (void*) data_master);

    /* Main (monitor) loop */
    for(mon_count=0; on; mon_count +=1) {

        usleep(cfg->mon_rate);
    }

    pthread_join(node_listen, NULL);

    clear_data_master(data_master);

    return EXIT_SUCCESS;
}
