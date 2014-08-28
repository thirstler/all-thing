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
#include "at.h"

#define BACKLOG 10
extern char on;
extern master_config_t *cfg;

#define CONN_CLOSED 1
#define CONN_OPEN 2


static char* parse_query(json_t *query, master_global_data_t *dptr)
{
	obj_rec_t *qresult;
	uint64_t id;
	/* boatloads of json object for juggling the query */
	json_t	*outter_val, \
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
	answerobj = json_pack("{s:{s:i,s:i}}",
			"ts","tv_sec", ts.tv_sec, "tv_usec", ts.tv_usec);
	answerobjs = json_object();
	json_object_set(answerobj, "result", answerobjs);
	for(o_index = 0; o_index < json_array_size(hostarr); o_index += 1) {
		outter_val = json_array_get(hostarr, o_index);

		sscanf(json_string_value(outter_val), "%lx", &id);
		qresult = get_obj_rec(id, dptr);
		if(qresult == NULL) continue;
		o_objtmp = json_object();
		for(i_index = 0; i_index < json_array_size(queryarr); i_index += 1) {
			inner_val = json_array_get(queryarr, i_index);

			strtmp = json_string_value(inner_val);

			/* Special case */
			if(strcmp(strtmp, "_ALL_") == 0) {
				json_decref(o_objtmp);
				o_objtmp = qresult->record;
				break;
			} else {
				i_objtmp = json_object_get(qresult->record,
						json_string_value(inner_val));
				if(i_objtmp == NULL) continue;
				json_object_set(o_objtmp, strtmp, i_objtmp);
			}
		}
		if(json_object_size(o_objtmp) > 0) {
			json_object_set(answerobjs, json_string_value(outter_val), o_objtmp);
		} else {
			json_decref(o_objtmp);
		}
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
		json_object_set(jobjbf, "id",
				json_object_get(dptr->obj_rec[i]->record, "hostid"));
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

		/* Query data objects */
		if( strncmp(buf, "query:", 6) == 0) {
			printf("%s\n", buf+6);
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
    master_global_data_t *data_master = dptr;
    fd_set master;
    fd_set read_fds;
    int fdmax;
    int listener;
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
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) continue;

        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }
        break;
    }

    if (p == NULL) {
        syslog(LOG_ERR, "selectserver: failed to bind");
        pthread_exit(NULL);
    }

    freeaddrinfo(ai);

    if (listen(listener, 10) == -1) {
        perror("listen");
        pthread_exit(NULL);
    }

    FD_SET(listener, &master);

    fdmax = listener;

    while(on) {
        read_fds = master;
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            return NULL;
        }

        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == listener) {
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
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
