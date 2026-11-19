/*
 * Lab 6 - Babble server, MULTI-THREADED version (Stage 1 + Stage 2 + Stage 3).
 *
 * Threads:
 *   - main thread          : accepts new connections, starts one
 *                            COMMUNICATION thread per client (Stage 1)
 *   - communication thread : receives the client messages, parses them and
 *                            puts the commands in the command buffer
 *                            (= PRODUCER)
 *   - executor threads     : BABBLE_EXECUTOR_THREADS threads that take
 *                            commands from the buffer, run them and send the
 *                            answer (= CONSUMERS) (Stage 2)
 *
 * Synchronization (summary, details in the comments):
 *   - command buffer          : producer/consumer, mutex + 2 cond vars
 *   - registration_table      : readers/writer lock (babble_registration.c)
 *   - followers list / timeline of a client : one mutex each
 *                               (babble_server_implem.c)
 *   - client socket           : send_lock, one answer at a time
 *                               (babble_server_answer.c)
 *   - RDV (step 2.d)          : per-client "pending" counter; the RDV ack is
 *                               sent only when all previous commands of the
 *                               client are finished
 *   - Stage 3                 : 2 buffers, PUBLISH buffer is served first
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>

#include "babble_server.h"
#include "babble_types.h"
#include "babble_utils.h"
#include "babble_communication.h"
#include "babble_server_answer.h"
#include "fastrand.h"
#include "babble_registration.h"


/* to activate random delays in the processing of messages */
int random_delay_activated;


static void display_help(char *exec)
{
    printf("Usage: %s -p port_number -r [activate_random_delays]\n", exec);
}


static int parse_command(char* str, command_t *cmd)
{
    char *name = NULL;
    
    /* start by cleaning the input */
    str_clean(str);
    
    /* get command id */
    cmd->cid=str_to_command(str, &cmd->answer_expected);
    
    switch(cmd->cid){
    case LOGIN:
        if(str_to_payload(str, cmd->msg, BABBLE_ID_MSIZE)){
            name = get_name_from_key(cmd->key);
            fprintf(stderr,"Error from [%s]-- invalid LOGIN -> %s\n", name, str);
            free(name);
            return -1;
        }
        break;
    case PUBLISH:
        if(str_to_payload(str, cmd->msg, BABBLE_PUBLICATION_MSIZE)){
            name = get_name_from_key(cmd->key);
            fprintf(stderr,"Warning from [%s]-- invalid PUBLISH -> %s\n", name, str);
            free(name);
            return -1;
        }
        break;
    case FOLLOW:
        if(str_to_payload(str, cmd->msg, BABBLE_ID_MSIZE)){
            name = get_name_from_key(cmd->key);
            fprintf(stderr,"Warning from [%s]-- invalid FOLLOW -> %s\n", name, str);
            free(name);
            return -1;
        }
        break;
    case TIMELINE:
        cmd->msg[0]='\0';
        break;
    case FOLLOW_COUNT:
        cmd->msg[0]='\0';
        break;
    case RDV:
        cmd->msg[0]='\0';
        break;    
    default:
        name = get_name_from_key(cmd->key);
        fprintf(stderr,"Error from [%s]-- invalid client command -> %s\n", name, str);
        free(name);
        return -1;
    }

    return 0;
}


/* processes the command and eventually generates an answer */
static int process_command(command_t *cmd, answer_t **answer)
{
    int res=0;

    switch(cmd->cid){
    case LOGIN:
        res = run_login_command(cmd, answer);
        break;
    case PUBLISH:
        random_delay(random_delay_activated);
        res = run_publish_command(cmd, answer);
        break;
    case FOLLOW:
        random_delay(random_delay_activated);
        res = run_follow_command(cmd, answer);
        break;
    case TIMELINE:
        random_delay(random_delay_activated);
        res = run_timeline_command(cmd, answer);
        break;
    case FOLLOW_COUNT:
        res = run_fcount_command(cmd, answer);
        break;
    case RDV:
        res = run_rdv_command(cmd, answer);
        break;
    case UNREGISTER:
        res = unregisted_client(cmd);
        *answer = NULL;
        break;
    default:
        fprintf(stderr,"Error -- Unknown command id\n");
        return -1;
    }

    if(res){
        fprintf(stderr,"Error -- Failed to run command ");
        display_command(cmd, stderr);
    }

    return res;
}



/* ======================================================================
 *  COMMAND BUFFER (producer / consumer)
 *
 *  Stage 1/2: a circular buffer of BABBLE_PRODCONS_SIZE commands.
 *  Stage 3  : TWO such buffers (BABBLE_PRODCONS_NB = 2):
 *               buffer 0 = PUBLISH commands (high priority)
 *               buffer 1 = all the other commands
 *             An executor always takes from buffer 0 first if it is not
 *             empty -> PUBLISH commands are processed "as soon as possible".
 *  One mutex protects both buffers. Condition variables:
 *    - not_full[b] : producers wait here when buffer b is full
 *    - not_empty   : executors wait here when BOTH buffers are empty
 * ====================================================================== */

#define PUBLISH_BUF 0
#define OTHER_BUF   (BABBLE_PRODCONS_NB - 1)  /* = 0 if only one buffer */

typedef struct {
    command_t *cmds[BABBLE_PRODCONS_SIZE];
    int in, out, count;
} cmd_buffer_t;

static cmd_buffer_t    buffers[BABBLE_PRODCONS_NB];
static pthread_mutex_t buf_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  not_full[BABBLE_PRODCONS_NB];
static pthread_cond_t  not_empty = PTHREAD_COND_INITIALIZER;

static void buffers_init(void)
{
    for(int b = 0; b < BABBLE_PRODCONS_NB; b++){
        buffers[b].in = buffers[b].out = buffers[b].count = 0;
        pthread_cond_init(&not_full[b], NULL);
    }
}

/* called by a communication thread (producer) */
static void buffer_put(command_t *cmd)
{
    int b = (cmd->cid == PUBLISH) ? PUBLISH_BUF : OTHER_BUF;

    pthread_mutex_lock(&buf_lock);
    while(buffers[b].count == BABBLE_PRODCONS_SIZE)        /* full -> wait */
        pthread_cond_wait(&not_full[b], &buf_lock);

    buffers[b].cmds[buffers[b].in] = cmd;
    buffers[b].in = (buffers[b].in + 1) % BABBLE_PRODCONS_SIZE;
    buffers[b].count++;

    pthread_cond_signal(&not_empty);                       /* wake 1 executor */
    pthread_mutex_unlock(&buf_lock);
}

/* called by an executor thread (consumer) */
static command_t* buffer_get(void)
{
    pthread_mutex_lock(&buf_lock);
    /* wait while ALL buffers are empty */
    while(1){
        int total = 0;
        for(int b = 0; b < BABBLE_PRODCONS_NB; b++) total += buffers[b].count;
        if(total > 0) break;
        pthread_cond_wait(&not_empty, &buf_lock);
    }

    /* Stage 3: first non-empty buffer, buffer 0 (PUBLISH) has priority */
    int b = 0;
    while(buffers[b].count == 0) b++;

    command_t *cmd = buffers[b].cmds[buffers[b].out];
    buffers[b].out = (buffers[b].out + 1) % BABBLE_PRODCONS_SIZE;
    buffers[b].count--;

    pthread_cond_signal(&not_full[b]);                     /* wake 1 producer */
    pthread_mutex_unlock(&buf_lock);
    return cmd;
}

/* ======================================================================
 *  RDV support (step 2.d)
 *
 *  Problem: with several executors, the commands of ONE client can be run
 *  by different threads at the same time. When the RDV arrives, an earlier
 *  PUBLISH of the same client may still be running in another executor, so
 *  the "rdv_ack" could be sent BEFORE that PUBLISH is done -> wrong.
 *
 *  Solution: each client has a counter "pending" = number of its commands
 *  in the buffer or being executed.
 *    - the communication thread does pending++ before putting a command
 *    - the executor does pending-- after running it (and wakes waiters
 *      when it reaches 0)
 *    - for RDV, the communication thread itself waits until pending == 0
 *      and then sends the ack. (Doing the wait in the communication thread
 *      means no executor thread is blocked, so no deadlock is possible.)
 * ====================================================================== */

static void pending_inc(client_bundle_t *c)
{
    pthread_mutex_lock(&c->pending_lock);
    c->pending++;
    pthread_mutex_unlock(&c->pending_lock);
}

static void pending_dec(client_bundle_t *c)
{
    pthread_mutex_lock(&c->pending_lock);
    c->pending--;
    if(c->pending == 0)
        pthread_cond_broadcast(&c->pending_zero);
    pthread_mutex_unlock(&c->pending_lock);
}

static void pending_wait_zero(client_bundle_t *c)
{
    pthread_mutex_lock(&c->pending_lock);
    while(c->pending > 0)
        pthread_cond_wait(&c->pending_zero, &c->pending_lock);
    pthread_mutex_unlock(&c->pending_lock);
}

/* ======================================================================
 *  EXECUTOR THREAD (Figure 3)
 * ====================================================================== */
static void* executor_thread(void *arg)
{
    (void)arg;
    answer_t *answer = NULL;

    /* per-thread random generator, must be seeded in EACH executor thread */
    fastRandomSetSeed(time(NULL) + pthread_self()*100);

    while(1){
        command_t *cmd = buffer_get();                 /* take a command */

        answer = NULL;
        if(process_command(cmd, &answer) == -1){       /* run it */
            fprintf(stderr, "Warning: unable to process command from client %lu\n", cmd->key);
        }
        if(send_answer_to_client(answer) == -1){       /* answer the client */
            fprintf(stderr, "Warning: unable to answer command from client %lu\n", cmd->key);
        }
        free_answer(answer);

        client_bundle_t *c = registration_lookup(cmd->key);
        if(c) pending_dec(c);                          /* RDV bookkeeping */
        free(cmd);
    }
    return NULL;
}

/* ======================================================================
 *  COMMUNICATION THREAD (Figure 2) - one per client
 * ====================================================================== */
static void* communication_thread(void *arg)
{
    int newsockfd = (int)(long) arg;
    char* recv_buff = NULL;
    int recv_size = 0;
    unsigned long client_key = 0;
    char client_name[BABBLE_ID_MSIZE+1];
    command_t *cmd;
    answer_t *answer = NULL;

    /* detached: nobody will pthread_join this thread, its resources are
       freed automatically when it ends */
    pthread_detach(pthread_self());

    memset(client_name, 0, BABBLE_ID_MSIZE+1);

    /* 1. receive and run the LOGIN directly in this thread */
    if((recv_size = network_recv(newsockfd, (void**)&recv_buff)) < 0){
        fprintf(stderr, "Error -- recv from client\n");
        close(newsockfd);
        return NULL;
    }

    cmd = new_command(0);
    if(parse_command(recv_buff, cmd) == -1 || cmd->cid != LOGIN){
        fprintf(stderr, "Error -- in LOGIN message\n");
        close(newsockfd);
        free(cmd);
        free(recv_buff);
        return NULL;
    }
    cmd->sockfd = newsockfd;

    if(process_command(cmd, &answer) == -1){
        fprintf(stderr, "Error -- in LOGIN\n");
        close(newsockfd);
        free(cmd);
        free(recv_buff);
        return NULL;
    }
    if(send_answer_to_client(answer) == -1){
        fprintf(stderr, "Error -- in LOGIN ack\n");
        close(newsockfd);
        free(cmd);
        free(recv_buff);
        free_answer(answer);
        return NULL;
    }
    free_answer(answer);

    client_key = cmd->key;
    strncpy(client_name, cmd->msg, BABBLE_ID_MSIZE);
    free(recv_buff);
    free(cmd);

    /* our own client struct (never freed, so the pointer stays valid) */
    client_bundle_t *me = registration_lookup(client_key);

    /* 2. loop: parse each message and give it to the executors */
    while((recv_size = network_recv(newsockfd, (void**) &recv_buff)) > 0){
        cmd = new_command(client_key);

        if(parse_command(recv_buff, cmd) == -1){
            fprintf(stderr, "Warning: unable to parse message from client %s\n", client_name);
            notify_parse_error(cmd, recv_buff, &answer);
            send_answer_to_client(answer);
            free_answer(answer);
            free(cmd);
        }
        else if(cmd->cid == RDV){
            /* step 2.d: wait until all previous commands are done, then ack */
            pending_wait_zero(me);
            answer = NULL;
            process_command(cmd, &answer);
            send_answer_to_client(answer);
            free_answer(answer);
            free(cmd);
        }
        else{
            pending_inc(me);
            buffer_put(cmd);          /* the executor will free cmd */
        }
        free(recv_buff);
    }

    /* 3. client left: wait until its commands are finished, then unregister.
       (Otherwise an executor could run a command of a removed client.) */
    if(client_name[0] != 0){
        pending_wait_zero(me);
        cmd = new_command(client_key);
        cmd->cid = UNREGISTER;
        if(process_command(cmd, &answer) == -1){
            fprintf(stderr,"Warning -- failed to unregister client %s\n",client_name);
        }
        free(cmd);
    }
    return NULL;
}

/* ======================================================================
 *  MAIN THREAD (Figure 1)
 * ====================================================================== */
int main(int argc, char *argv[])
{
    int sockfd, newsockfd;
    int portno=BABBLE_PORT;
    int opt;
    int nb_args=1;
    pthread_t tid;

    while ((opt = getopt (argc, argv, "+hp:r")) != -1){
        switch (opt){
        case 'p':
            portno = atoi(optarg);
            nb_args+=2;
            break;
        case 'r':
            random_delay_activated=1;
            nb_args+=1;
            break;
        case 'h':
        case '?':
        default:
            display_help(argv[0]);
            return -1;
        }
    }

    if(nb_args != argc){
        display_help(argv[0]);
        return -1;
    }

    server_data_init();
    buffers_init();

    /* start the executor threads */
    for(int i = 0; i < BABBLE_EXECUTOR_THREADS; i++){
        if(pthread_create(&tid, NULL, executor_thread, NULL) != 0){
            fprintf(stderr, "Error -- cannot create executor thread\n");
            return -1;
        }
    }

    if((sockfd = server_connection_init(portno)) == -1){
        return -1;
    }

    printf("Babble server bound to port %d\n", portno);

    /* main server loop: accept clients, one communication thread each */
    while(1){
        if((newsockfd = server_connection_accept(sockfd)) == -1){
            return -1;
        }
        /* pass the socket number itself (cast), not a pointer to a local
           variable that would change at the next accept */
        if(pthread_create(&tid, NULL, communication_thread, (void*)(long)newsockfd) != 0){
            fprintf(stderr, "Error -- cannot create communication thread\n");
            close(newsockfd);
        }
    }

    close(sockfd);
    return 0;
}
