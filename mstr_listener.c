#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <syslog.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "at.h"

extern char on;
extern master_config_t *cfg;

static int destroy_assmbl_buf(
        unsigned int id,
        json_str_assembly_bfr_t **ab)
{
    json_str_assembly_bfr_t *ref = *ab;
    json_str_assembly_bfr_t *prev = NULL;

    while(ref != NULL) {
        if(ref->id == id) {
            if(prev == NULL) {
                /* first item! (could be only) */
                *ab = ref->next;
            } else {
                /* everywhere else (could be last) */
                prev->next = ref->next;
            }
            free(ref->json_str);
            free(ref);
            return EXIT_SUCCESS;
        }
        prev = ref;
        ref = ref->next;
    }
    return EXIT_FAILURE;
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
 * Get an assembly buffer for a node ID, returns NULL when unable to find
 */
static inline json_str_assembly_bfr_t* get_assmbl_buf(
        const rprt_hdr_t *hdr,
        json_str_assembly_bfr_t **buffer_list)
{
    json_str_assembly_bfr_t *bfrptr = *buffer_list;
    while(bfrptr != NULL) {
        if(hdr->hostid == bfrptr->id) {
            if(hdr->msgid == bfrptr->msgid) return bfrptr;
            else {
                /* Message mix-up! */
                destroy_assmbl_buf(hdr->hostid, buffer_list);
                return NULL;
            }
        }
        bfrptr = bfrptr->next;
    }
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
        sysinf_t **system_data)
{
    sysinf_t *newobj = dataobj_from_jsonstr(
            assembly_buffer->id,
            assembly_buffer->json_str);


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
            destroy_assmbl_buf(abptr->id, assembly_buffer);
            continue;
        }
        abptr = abptr->next;
    }
}

void *report_listener(void *dptr)
{
    int rc;
    master_global_data_t *data_master = dptr;
    json_str_assembly_bfr_t *assembly_buffer = NULL; // start of linked-list
    json_str_assembly_bfr_t *this_asmbl_buf;         // working buffer
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct sockaddr fromaddr;
    socklen_t fromsz = sizeof(struct sockaddr);
    int lsock;
    uint64_t pktc;
    char recv_buffer[UDP_SEND_SZ+1];
    char strbuf[255];
    rprt_hdr_t pkt_hdr;

    /* Fire-up a socket */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rc = getaddrinfo(NULL, cfg->listen_port, &hints, &servinfo)) != 0) {
        syslog(LOG_ERR, "getaddrinfo error: %s", gai_strerror(rc));
        exit(1);
    }

    lsock = socket(servinfo->ai_family,
            servinfo->ai_socktype,
            servinfo->ai_protocol);

    if(lsock < 0) {
        syslog(LOG_ERR, "listener failed to get socket");
        return NULL;
    }

    rc = bind(lsock, servinfo->ai_addr, servinfo->ai_addrlen);
    if(rc < 0) {
        syslog(LOG_ERR, "listener failed to bind to socket");
        return NULL;
    }

    freeaddrinfo(servinfo);

    for(pktc=0; on; pktc +=1) {

        memset(recv_buffer, '\0', UDP_SEND_SZ+1);
        rc = recvfrom(lsock,
                (void*)recv_buffer,
                UDP_SEND_SZ, 0,
                &fromaddr, &fromsz);

        if(sscanf(recv_buffer,"%x %lx %lx %lx %lx",
                &pkt_hdr.hostid, &pkt_hdr.seq,
                &pkt_hdr.msg_sz, &pkt_hdr.offset, &pkt_hdr.msgid) == 5)
        {
            this_asmbl_buf = get_assmbl_buf(&pkt_hdr, &assembly_buffer);
            if(this_asmbl_buf == NULL)
                this_asmbl_buf = new_assmbl_buf(&pkt_hdr, &assembly_buffer);
            if(this_asmbl_buf == NULL) continue;

            if(pkt_assembly(&pkt_hdr,
                    (strstr(recv_buffer, "\n")+1),
                    this_asmbl_buf) == ASMBL_BUFFER_COMPLETE) {
                apply_assmb_buf(this_asmbl_buf, data_master->system_data);
                destroy_assmbl_buf(pkt_hdr.hostid, &assembly_buffer);
            }
            if( (pktc % ASMBL_BUF_PRUNE_COUNT) == 0)
                prune_assmbl_buf(&assembly_buffer);

        } else if(sscanf(recv_buffer, "<<%s exited>>", strbuf) == 1) {
            printf("host ID %s has stopped its agent\n", strbuf);
            continue;
        }  else {
            printf("bad packet?\n");
        }

    }

    while(assembly_buffer != NULL)
        destroy_assmbl_buf(assembly_buffer->id, &assembly_buffer);

    return NULL;
}
