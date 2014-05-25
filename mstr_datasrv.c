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
	char *strtmp, *result_str;

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
	json_array_foreach(hostarr, o_index, outter_val) {
		sscanf(json_string_value(outter_val), "%lx", &id);
		qresult = get_obj_rec(id, dptr);
		if(qresult == NULL) continue;
		o_objtmp = json_object();
		json_array_foreach(queryarr, i_index, inner_val) {
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
				json_object_set_new(o_objtmp, strtmp, i_objtmp);
			}
		}
		if(json_object_size(o_objtmp) > 0) {
			json_object_set_new(answerobjs, json_string_value(outter_val), o_objtmp);
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

static int query_handler(int fd, master_global_data_t *dptr)
{
	int nbytes, i;
	static char buf[1024];
	static struct rusage rusage;
	char *json_ptr;
	json_t *jquery, *jobjbf, *jarbf;
	json_error_t jerr;
	char *strresult;


	memset(buf, '\0', 1024);

	if ((nbytes = recv(fd, buf, sizeof buf, 0)) <= 0) {
		// got error or connection closed by client
		if (nbytes == 0) {
			// connection closed
			syslog(LOG_INFO,
					"data_Server: socket %d hung up", fd);
		} else {
			perror("recv");
		}
		close(fd); // bye!
		return CONN_CLOSED;

	} else {

		if(strcmp(buf, "list\n") == 0) {
			jarbf = json_array();
			for(i = 0; i < dptr->obj_rec_sz; i+=1) {
				jobjbf = json_object();
				json_object_set(jobjbf, "id",
						json_object_get(dptr->obj_rec[i]->record, "hostid"));
				json_object_set(jobjbf, "hostname",
						json_object_get(dptr->obj_rec[i]->record, "hostname"));
				json_array_append_new(jarbf, jobjbf);
			}
			json_ptr = json_dumps(jarbf, JSON_COMPACT);
			send(fd, json_ptr, strlen(json_ptr)+1, 0);
			free(json_ptr);
			json_array_clear(jarbf);
			json_decref(jarbf);
		}

		if( strcmp(buf, "stats\n") == 0) {
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

// get sockaddr, IPv4 or IPv6:
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
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, cfg->server_port, &hints, &ai)) != 0) {
        syslog(LOG_ERR, "report_listener: %s", gai_strerror(rv));
        pthread_exit(NULL);
    }

    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        syslog(LOG_ERR, "selectserver: failed to bind");
        pthread_exit(NULL);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        pthread_exit(NULL);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    while(on) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            return NULL;
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        syslog(LOG_INFO, "data_server: new connection from "
                        	"%s on socket %d",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                    }
                } else {
                    if( query_handler(i, data_master) == CONN_CLOSED ) {
                    	FD_CLR(i, &master); // remove from master set
                    }
                }
            }
        }
    }

    pthread_exit(NULL);
}
