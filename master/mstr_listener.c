#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <syslog.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <search.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libpq-fe.h>
#include <jansson.h>
#include <time.h>
#include <pthread.h>
#include "../at.h"
#include "at_master.h"

extern char on;
extern master_config_t *cfg;
extern pthread_mutex_t db_queue_ops_mtx;
int asmblerr;

#ifdef DDEBUG
static size_t get_ab_lll(json_str_assembly_bfr_t *ll)
{
	size_t count=0;
	while(ll != NULL) {
		ll = ll->next;
		count += 1;
	}
	return count;
}
#endif

static json_str_assembly_bfr_t* destroy_assmbl_buf(
        uint64_t id,
        json_str_assembly_bfr_t *ab)
{
    json_str_assembly_bfr_t *prev = NULL;
    json_str_assembly_bfr_t *ph = ab; /* hold the start of the list */

    while(ab != NULL) {
        if(ab->id == id) {
            if(prev == NULL) {
                /* first item! (could be only) */
            	ph = ab->next;
            } else {
                /* everywhere else (could be last) */
                prev->next = ab->next;
            }
            free(ab->json_str);
            free(ab);
            asmblerr = ASMBL_BUFFER_SUCCESS;
            return ph;
        }
        prev = ab;
        ab = ab->next;
    }
    /* Not found */
    asmblerr = ASMBL_BUFFER_NOT_FOUND;
    return ph;
}

static json_str_assembly_bfr_t* mk_new_assmbl_buf(
        const rprt_hdr_t *hdr)
{
    /* Create new assembly buffer */
    json_str_assembly_bfr_t *newbfr;
    newbfr = malloc(sizeof(json_str_assembly_bfr_t));
    memset(newbfr, '\0', sizeof(json_str_assembly_bfr_t));
    newbfr->id = hdr->hostid;
    newbfr->json_str = malloc(hdr->msg_sz+1);
    memset(newbfr->json_str, '\0', hdr->msg_sz+1);
    newbfr->strlen = hdr->msg_sz;
    gettimeofday(&newbfr->ts, NULL);
    newbfr->next = NULL;
    newbfr->msgid = hdr->msgid;

    return newbfr;
}

static json_str_assembly_bfr_t* new_assmbl_buf(
        const rprt_hdr_t *hdr,
        json_str_assembly_bfr_t **ab)
{
    json_str_assembly_bfr_t *newbfr = mk_new_assmbl_buf(hdr);
    json_str_assembly_bfr_t *ref = *ab;
    json_str_assembly_bfr_t *prev = NULL;

    while(ref != NULL) {
        if(ref->id > hdr->hostid) {
            /* It's the first entry! */
            if(prev == NULL) {
                newbfr->next = ref;
                *ab = newbfr;
            } else {
                newbfr->next = ref;
                prev->next = newbfr;
            }
            return newbfr;
        }
        prev = ref;
        ref = ref->next;
    }
    /* It's the only entry! */
    if(prev == NULL) *ab = newbfr;

    /* Its the last entry! */
    else prev->next = newbfr;

    return newbfr;
}

/**
 * Get an assembly buffer for a node ID, returns NULL when unable to find and
 * sets assmblerr appropriately.
 */
static inline json_str_assembly_bfr_t* get_assmbl_buf(
        const rprt_hdr_t *hdr,
        json_str_assembly_bfr_t *bfrptr)
{
    while(bfrptr != NULL) {
        if(hdr->hostid == bfrptr->id) {
            if(hdr->msgid == bfrptr->msgid) {
            	asmblerr = ASMBL_BUFFER_FOUND;
            	return bfrptr;
            } else {
                /* Message mix-up! */
                asmblerr = ASMBL_BUFFER_BAD_MSG;
                return NULL;
            }
        }
        bfrptr = bfrptr->next;
    }
    asmblerr = ASMBL_BUFFER_NOT_FOUND;
    return NULL;
}

static int pkt_assembly(
        rprt_hdr_t *hdr,
        char *msg_str,
        json_str_assembly_bfr_t *assembly_buffer)
{
    char *ptr = assembly_buffer->json_str + hdr->offset;
    strcpy(ptr, msg_str);
    assembly_buffer->charcount += strlen(msg_str);

    if(assembly_buffer->charcount == hdr->msg_sz) {
        return(ASMBL_BUFFER_COMPLETE);
    } else {
        return(ASMBL_BUFFER_INCOMPLETE);
    }
}

static void apply_assmb_buf(
        json_str_assembly_bfr_t *assembly_buffer,
        master_global_data_t *data_master)
{
	json_error_t error;
	obj_rec_t *standing;
	obj_rec_t *incomming = malloc(sizeof(obj_rec_t));
	time_t nownow = time(NULL);
	const char *rate_list[] = CALC_RATE_KEYS;

	incomming->record = json_loads(assembly_buffer->json_str, 0, &error);

    if(incomming->record == NULL) {
    	syslog(LOG_WARNING,
    			"failed to parse JSON string from node id %lx, message id %lx",
    			assembly_buffer->id, assembly_buffer->msgid);
    	free(incomming);
    	return;
    }
    incomming->id = assembly_buffer->id;

    syslog(LOG_DEBUG, "successfully encoded JSON object with id %lx",
    		incomming->id);

    if(incomming->id == 0) goto cleanup;

    standing = get_obj_rec(incomming->id, data_master);

	if(standing == NULL) {
		add_obj_rec(data_master, incomming);
		/* Write to cache in-line (no queue) when it's a first occurrence */
		write_agent_to_cache(incomming, 1);
		init_record_tbl(incomming);
		//printf("%s\n", json_dumps(incomming->record, JSON_INDENT(2)));
	} else {

		json_object_set_new(standing->record, "rates",
				get_these_rates(standing->record, incomming->record,
						rate_list, 4));
		json_object_update(standing->record, incomming->record);
		free_obj_rec(incomming);

		cache_update_to_db_ops_queue(standing, data_master->data_ops_queue);

		if( (nownow - standing->last_commit) >= standing->commit_rate ) {
			standing->last_commit = nownow;
			db_commit_to_db_ops_queue(standing, data_master->data_ops_queue);
		}
	}

	return;

	cleanup:
	syslog(LOG_WARNING, "failed to process valid JSON object with id %lx",
			incomming->id);
	free_obj_rec(incomming);
    return;
}

static void prune_assmbl_buf(json_str_assembly_bfr_t **assembly_buffer)
{
    json_str_assembly_bfr_t *abptr = *assembly_buffer;
    struct timeval nownow;
    struct timeval age;
    gettimeofday(&nownow, NULL);

    while(abptr != NULL) {
        timersub(&nownow, &abptr->ts, &age);
        if(age.tv_sec > MASTER_ASMBL_BUF_AGE) {
            abptr = abptr->next;
            *assembly_buffer = destroy_assmbl_buf(abptr->id, *assembly_buffer);
            continue;
        }
        abptr = abptr->next;
    }
}

void *report_listener(void *dptr)
{
    int rc;
    listener_in_t *listener_in = dptr;
    master_global_data_t *data_master = listener_in->master;

    /* Message assembly buffer */
    json_str_assembly_bfr_t *asmbl_bfr_list = NULL;  // start of linked-list
    json_str_assembly_bfr_t *this_asmbl_buf;         // working buffer

    /* Network listening */
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct sockaddr fromaddr;
    socklen_t fromsz = sizeof(struct sockaddr);

    uint64_t pktc;
    char recv_buffer[UDP_PAYLOAD_SZ+1];
    char strbuf[255];
    rprt_hdr_t pkt_hdr;

    /* Fire-up a socket */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    /* Set wild card address*/
    if( strcmp(cfg->listen_addr, "*") == 0) {
    	free(cfg->listen_addr);
    	cfg->listen_addr = strdup("0.0.0.0");
    }

    if ((rc = getaddrinfo(cfg->listen_addr, cfg->listen_port, &hints, &servinfo)) != 0) {
        syslog(LOG_ERR, "getaddrinfo error: %s", gai_strerror(rc));
        exit(1);
    }

    listener_in->lsn_socket = socket(servinfo->ai_family,
            servinfo->ai_socktype,
            servinfo->ai_protocol);

    if(listener_in->lsn_socket < 0) {
        syslog(LOG_ERR, "listener failed to get socket");
        pthread_exit(NULL);
    }

    rc = bind(listener_in->lsn_socket, servinfo->ai_addr, servinfo->ai_addrlen);
    if(rc < 0) {
        syslog(LOG_ERR, "listener failed to bind to socket");
        pthread_exit(NULL);
    }

    freeaddrinfo(servinfo);

    /* Forget that dynamic shit, just allocate the array */
    data_master->obj_rec = malloc(sizeof(obj_rec_t*)*DATA_STRUCT_AR_SIZE);
    memset(data_master->obj_rec, '\0', sizeof(obj_rec_t*)*DATA_STRUCT_AR_SIZE);
    data_master->obj_rec_sz = 0;

    syslog(LOG_INFO, "listener thread launched");

    pktc=0;
    while(on) {

    	/* clear the buffer before using it */
        memset(recv_buffer, '\0', UDP_PAYLOAD_SZ+1);

        rc = recvfrom(listener_in->lsn_socket,
                (void*)recv_buffer,
                UDP_PAYLOAD_SZ, 0,
                &fromaddr, &fromsz);

        /* Read packet header */
        if(sscanf(recv_buffer,"%lx %lx %lx %lx %lx",
                &pkt_hdr.hostid, &pkt_hdr.seq,
                &pkt_hdr.msg_sz, &pkt_hdr.offset, &pkt_hdr.msgid) == 5)
        {
            this_asmbl_buf = get_assmbl_buf(&pkt_hdr, asmbl_bfr_list);
            if(asmblerr != ASMBL_BUFFER_FOUND) {

            	/* host ID not found in buffer, must need a new one */
            	if(asmblerr == ASMBL_BUFFER_NOT_FOUND) {
            		this_asmbl_buf = new_assmbl_buf(&pkt_hdr, &asmbl_bfr_list);

            	/* out-of-order message ID, dump current buffer and start a
            	 * new one */
            	} else if(asmblerr == ASMBL_BUFFER_BAD_MSG) {
            		asmbl_bfr_list = destroy_assmbl_buf(
            								pkt_hdr.hostid, asmbl_bfr_list);
            		this_asmbl_buf = new_assmbl_buf(&pkt_hdr, &asmbl_bfr_list);
            	}

            }

            /* If packet assembly for this host message is done, update the
             * appropriate data structure */
            if(pkt_assembly(&pkt_hdr,
                    (strstr(recv_buffer, "\n")+1),
                    this_asmbl_buf) == ASMBL_BUFFER_COMPLETE) {

            	/* apply buffered message to data struct */
                apply_assmb_buf(this_asmbl_buf, data_master);

                /* clean up assembly buffer for this message */
                asmbl_bfr_list = destroy_assmbl_buf(
                						pkt_hdr.hostid, asmbl_bfr_list);
            }

            if( (pktc % ASMBL_BUF_PRUNE_COUNT) == 0)
                prune_assmbl_buf(&asmbl_bfr_list);

        } else if (strncmp(recv_buffer, "MARK", 4) == 0) {
        	continue;
        } else if(sscanf(recv_buffer, "<<%s exited>>", strbuf) == 1) {
            syslog(LOG_INFO, "host ID %s has stopped its agent\n", strbuf);
        }  else {
        	syslog(LOG_DEBUG, "foreign packet dropped");
        }
        pktc +=1;
    }

    while(asmbl_bfr_list != NULL)
    	asmbl_bfr_list = destroy_assmbl_buf(
    									asmbl_bfr_list->id, asmbl_bfr_list);

    syslog(LOG_CRIT, "node listener thread exited");
    pthread_exit(NULL);
}
