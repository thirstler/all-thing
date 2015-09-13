/*
 * File: mstr_datasrv.c
 * Desc: Data server thread entry and supporting functions
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <libpq-fe.h>
#include <jansson.h>
#include <pthread.h>
#include <uuid/uuid.h>
#include "../at.h"
#include "at_master.h"

#define BACKLOG 10
extern char on;
extern master_config_t *cfg;

#define CONN_CLOSED 1
#define CONN_OPEN 2

static json_t *getpath(const char *path, json_t *root)
{
    const char s[2] = ".";
    char *elm;
    char *srccpy = strdup(path);

    json_t *json_ptr = root;

    /* get the first token */
    elm = strtok( (char*)srccpy, s);


    /* walk through other tokens */
    while( elm != NULL ) {
        json_ptr = json_object_get(json_ptr, elm);
        elm = strtok(NULL, s);
    }
    free(srccpy);
    return json_ptr;
}

static char* parse_query(json_t *query, master_global_data_t *dptr)
{
    obj_rec_t *qresult;
    uuid_t uuid;
    /* boatloads of json object for juggling the query */
    json_t  *outter_val, \
            *inner_val, \
            *answerobj, \
            *answerobjs, \
            *o_objtmp, \
            *i_objtmp, \
            *hostarr = json_object_get(query, "hostid"),
            *queryarr = json_object_get(query, "get");
    size_t o_index, i_index;
    struct timeval ts;
    const char *strtmp;
    char *result_str;

    if(hostarr == NULL) {
        return strdup("{\"ERROR\":\"no 'hostid' present in query\"}");
    }
    if(json_is_array(hostarr) == 0) {
        return strdup(
            "{\"ERROR\":\"'hostid' found in query but it's not an array\"}");
    }

    if(queryarr == NULL) {
        return strdup("{\"ERROR\":\"no 'get' present in query\"}");
    }
    if(json_is_array(queryarr) == 0) {
        return strdup(
            "{\"ERROR\":\"'get' found in query but it's not an array\"}");
    }

    gettimeofday(&ts, NULL);

    /* Create outer object w/time */
    answerobj = json_pack("{s:{s:i,s:i}}",
            "ts","tv_sec", ts.tv_sec, "tv_usec", ts.tv_usec);

    /* created results object */
    answerobjs = json_object();
    json_object_set(answerobj, "result", answerobjs);

    for(o_index = 0; o_index < json_array_size(hostarr); o_index += 1) {
        
        uuid_parse(json_string_value(
                outter_val = json_array_get(hostarr, o_index)), uuid);
        
        /* Find that host in the master data object or go home*/
        if ( (qresult = get_obj_rec(uuid, dptr)) == NULL ) continue;

        /* Create response object for this host */
        o_objtmp = json_object();

        /* Loop through the attribute requests */
        for(i_index = 0; i_index < json_array_size(queryarr); i_index += 1) {

            /* Fetch our attribute name string */
            strtmp = json_string_value(
                    inner_val = json_array_get(queryarr, i_index));

            /* If it's _ALL_, dump the whole object and break */
            if(strcmp(strtmp, "_ALL_") == 0) {
                json_object_set(o_objtmp, strtmp, qresult->record);
                break;

            /* Otherwise pick them out */
            } else {
                i_objtmp = getpath(strtmp, qresult->record);
                if(i_objtmp == NULL) continue;
                json_object_set(o_objtmp, strtmp, i_objtmp);
            }

        }

        if(json_object_size(o_objtmp) > 0) {
            json_object_set(answerobjs,
                    json_string_value(outter_val), o_objtmp);
        }

        json_decref(o_objtmp);
    }

    if(answerobj != NULL) {
        result_str = json_dumps(answerobj, JSON_COMPACT);
        json_decref(answerobjs);
        json_decref(answerobj);
        return result_str;
    }
    return NULL;
}

inline static void q_list_objs(int fd, master_global_data_t *dptr)
{
    json_t *jobjbf;
    json_t *jarbf = json_array();
    size_t i;
    char *json_ptr;

    for(i = 0; i < dptr->obj_rec_sz; i+=1) {
        jobjbf = json_object();
        json_object_set(jobjbf, "uuid",
                json_object_get(dptr->obj_rec[i]->record, "uuid"));
        json_object_set(jobjbf, "hostname",
                json_object_get(
                        json_object_get(dptr->obj_rec[i]->record,
                                "misc"), "hostname"));
        json_array_append_new(jarbf, jobjbf);
    }
    json_ptr = json_dumps(jarbf, JSON_COMPACT);
    send(fd, json_ptr, strlen(json_ptr)+1, 0);
    free(json_ptr);
    json_array_clear(jarbf);
    json_decref(jarbf);
}

inline static void q_show_stats(int fd, master_global_data_t *dptr)
{
    static struct rusage rusage;
    json_t *jobjbf;
    char *json_ptr;
    getrusage( RUSAGE_SELF, &rusage );
    jobjbf = json_pack("{sIsIsIsIsIsIsIsIsI}",
            "object count", dptr->obj_rec_sz,
            "max rss", rusage.ru_maxrss,
            "minflt",rusage.ru_minflt,
            "majflt", rusage.ru_majflt,
            "ru_inblock", rusage.ru_inblock,
            "ru_oublock", rusage.ru_oublock,
            "ru_nvcsw", rusage.ru_nvcsw,
            "ru_nivcsw", rusage.ru_nivcsw,
            "shared mem size", rusage.ru_ixrss,
            "unshared data size", rusage.ru_idrss,
            "unshared stack size", rusage.ru_isrss,
            "system time sec", rusage.ru_stime.tv_sec,
            "system time usec", rusage.ru_stime.tv_usec,
            "user time sec", rusage.ru_utime.tv_sec,
            "user time usec", rusage.ru_utime.tv_usec);
    json_ptr = json_dumps(jobjbf, JSON_COMPACT);
    if(json_ptr != NULL) {
        send(fd, json_ptr, strlen(json_ptr)+1, 0);
        free(json_ptr);
    }
    json_decref(jobjbf);
}

inline static void q_show_mark(int fd) {send(fd, "ahoy!\n\0", 7, 0);}

inline static void q_show_config(int fd, master_global_data_t *dptr)
{
    json_t *jobjbf = NULL;
    char *json_ptr;
    jobjbf = json_pack("{sssIsssssssisisIss}",
            "config dir", cfg->config_dir,
            "daemon", cfg->daemon,
            "database name", cfg->db_db,
            "database host", cfg->db_host,
            "database user", cfg->db_user,
            "commit rate", cfg->def_commit_rate,
            "log level", cfg->log_level,
            "monitor rate", cfg->mon_rate,
            "run user", cfg->runuser);
    json_ptr = json_dumps(jobjbf, JSON_COMPACT);
    if(json_ptr != NULL) {
        send(fd, json_ptr, strlen(json_ptr)+1, 0);
        free(json_ptr);
    }
    json_decref(jobjbf);
}

static int query_handler(int fd, master_global_data_t *dptr)
{
    int nbytes;
    static char buf[1024];
    json_t *jquery;
    json_error_t jerr;
    char *strresult;

    memset(buf, '\0', 1024);

    if ((nbytes = recv(fd, buf, sizeof buf, 0)) <= 0) {
        if (nbytes == 0) {
            syslog(LOG_INFO,
                    "data_Server: socket %d hung up", fd);
        } else {
            perror("recv");
        }
        close(fd);
        return CONN_CLOSED;

    } else {

        /* Various special queries */
        if( strcmp(buf, "list\n") == 0 ) q_list_objs(fd, dptr);
        if( strcmp(buf, "stats\n") == 0 ) q_show_stats(fd, dptr);
        if( strcmp(buf, "config\n") == 0 ) q_show_config(fd, dptr);
        if( strcmp(buf, "MARK\n") == 0 ) q_show_mark(fd);

        /* Query data objects */
        if( strncmp(buf, "query:", 6) == 0) {
            jquery = json_loads(buf+6, JSON_DISABLE_EOF_CHECK, &jerr);
            if(jquery == NULL) {
                syslog(LOG_WARNING,
                        "error loading query: %s at line %d, col %d (pos: %d)",
                        jerr.text, jerr.line, jerr.column, jerr.position);
            } else {
                strresult = parse_query(jquery, dptr);
                if(strresult != NULL) {
                    send(fd, strresult, strlen(strresult)+1, 0);
                    free(strresult);
                }
                json_decref(jquery);
            }
        }
    }
    return CONN_OPEN;
}

/* get sockaddr, IPv4 or IPv6: */
static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void *server_listener(void *dptr)
{

    data_server_in_t *data_srv_in = dptr;
    master_global_data_t *data_master = data_srv_in->master;
    fd_set master;
    fd_set read_fds;
    int fdmax;
    int newfd;
    struct sockaddr_storage remoteaddr;
    socklen_t addrlen;
    char remoteIP[INET6_ADDRSTRLEN];
    int yes=1;
    int i, rv;
    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    /* Set wild card address*/
    if( strcmp(cfg->server_addr, "*") == 0) {
        free(cfg->server_addr);
        cfg->server_addr = strdup("0.0.0.0");
    }

    if ((rv = getaddrinfo(cfg->server_addr,
                cfg->server_port, &hints, &ai)) != 0) {
        syslog(LOG_ERR, "report_listener: %s", gai_strerror(rv));
        pthread_exit(NULL);
    }

    for(p = ai; p != NULL; p = p->ai_next) {
        data_srv_in->ds_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (data_srv_in->ds_socket < 0) continue;

        setsockopt(data_srv_in->ds_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(data_srv_in->ds_socket, p->ai_addr, p->ai_addrlen) < 0) {
            close(data_srv_in->ds_socket);
            continue;
        }
        break;
    }

    if (p == NULL) {
        syslog(LOG_ERR, "selectserver: failed to bind");
        pthread_exit(NULL);
    }

    freeaddrinfo(ai);

    if (listen(data_srv_in->ds_socket, 10) == -1) {
        perror("listen");
        pthread_exit(NULL);
    }

    FD_SET(data_srv_in->ds_socket, &master);

    fdmax = data_srv_in->ds_socket;

    while(on) {
        read_fds = master;
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            return NULL;
        }

        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == data_srv_in->ds_socket) {
                    addrlen = sizeof remoteaddr;
                    newfd = accept(data_srv_in->ds_socket,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master);
                        if (newfd > fdmax) {
                            fdmax = newfd;
                        }
                        syslog(LOG_INFO, "data_server: new connection from "
                            "%s on socket %d",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN), newfd);
                    }
                } else {
                    if( query_handler(i, data_master) == CONN_CLOSED ) {
                        FD_CLR(i, &master);
                    }
                }
            }
        }
    }

    syslog(LOG_CRIT, "dataserver server thread exited");
    pthread_exit(NULL);
}
