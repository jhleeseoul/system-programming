/*---------------------------------------------------------------------------*/
/* server.c                                                                  */
/* Author: Junghan Yoon, KyoungSoo Park                                      */
/* Modified by: (Your Name)                                                  */
/*---------------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <sys/time.h>
#include "common.h"
#include "skvslib.h"
/*---------------------------------------------------------------------------*/
struct thread_args
{
    int listenfd;
    int idx;
    struct skvs_ctx *ctx;

/*---------------------------------------------------------------------------*/
    /* free to use */

/*---------------------------------------------------------------------------*/
};
/*---------------------------------------------------------------------------*/
volatile static sig_atomic_t g_shutdown = 0;
/*---------------------------------------------------------------------------*/
void *handle_client(void *arg)
{
    TRACE_PRINT();
    struct thread_args *args = (struct thread_args *)arg;
    struct skvs_ctx *ctx = args->ctx;
    int idx = args->idx;
    int listenfd = args->listenfd;
/*---------------------------------------------------------------------------*/
    /* free to declare any variables */
    
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    int clientfd;

/*---------------------------------------------------------------------------*/

    free(args);
    printf("%dth worker ready\n", idx);

/*---------------------------------------------------------------------------*/
    /* edit here */

    while (!g_shutdown) {
        // 클라이언트 연결 수락
        clientfd = accept(listenfd, NULL, NULL);
        if (clientfd < 0) {
            if (errno == EINTR && g_shutdown) {
                break; // 서버 종료 신호 수신 시 루프 종료
            }
            perror("accept failed");
            continue;
        }

        printf("Worker %d: Accepted new connection.\n", idx);

        // 클라이언트와 통신
        while ((bytes_received = recv(clientfd, buffer, BUFFER_SIZE, 0)) > 0) {
            buffer[bytes_received] = '\0'; // Null-terminate buffer
            const char *response = skvs_serve(ctx, buffer, bytes_received);
            if (response) {
                char send_buf[BUFFER_SIZE];
                snprintf(send_buf, sizeof(send_buf), "%s\n", response);
                send(clientfd, send_buf, strlen(send_buf), 0);
            }
        }

        // 클라이언트 연결 종료
        if (bytes_received == 0) {
            printf("Worker %d: Client disconnected.\n", idx);
        } else if (bytes_received < 0) {
            perror("recv failed");
        }
        close(clientfd);
    }

    printf("Worker %d: Shutting down.\n", idx);

/*---------------------------------------------------------------------------*/

    return NULL;
}
/*---------------------------------------------------------------------------*/
/* Signal handler for SIGINT */
void handle_sigint(int sig)
{
    TRACE_PRINT();
    printf("\nReceived SIGINT, initiating shutdown...\n");
    g_shutdown = 1;
}
/*---------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    size_t hash_size = DEFAULT_HASH_SIZE;
    char *ip = DEFAULT_ANY_IP;
    int port = DEFAULT_PORT, opt;
    int num_threads = NUM_THREADS;
    int delay = RWLOCK_DELAY;
/*---------------------------------------------------------------------------*/
    /* free to declare any variables */

    int listenfd;
    struct sockaddr_in server_addr;
    struct skvs_ctx *ctx;

    pthread_t *threads;
    struct thread_args *args;

    signal(SIGINT, handle_sigint);
    
/*---------------------------------------------------------------------------*/

    /* parse command line options */
    while ((opt = getopt(argc, argv, "p:t:s:d:h")) != -1)
    {
        switch (opt)
        {
        case 'p':
            port = atoi(optarg);
            break;
        case 't':
            num_threads = atoi(optarg);
            break;
        case 's':
            hash_size = atoi(optarg);
            if (hash_size <= 0)
            {
                perror("Invalid hash size");
                exit(EXIT_FAILURE);
            }
            break;
        case 'd':
            delay = atoi(optarg);
            break;
        case 'h':
        default:
            printf("Usage: %s [-p port (%d)] "
                   "[-t num_threads (%d)] "
                   "[-d rwlock_delay (%d)] "
                   "[-s hash_size (%d)]\n",
                   argv[0],
                   DEFAULT_PORT,
                   NUM_THREADS,
                   RWLOCK_DELAY,
                   DEFAULT_HASH_SIZE);
            exit(EXIT_FAILURE);
        }
    }

/*---------------------------------------------------------------------------*/
    /* edit here */
    
    /* SKVS 초기화 */
    ctx = skvs_init(hash_size, delay);
    if (!ctx) {
        fprintf(stderr, "Failed to initialize SKVS.\n");
        exit(EXIT_FAILURE);
    }

    /* 서버 소켓 생성 */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(listenfd, NUM_BACKLOG) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d with %d threads.\n", port, num_threads);

    /* 쓰레드 풀 생성 */
    threads = malloc(num_threads * sizeof(pthread_t));
    for (int i = 0; i < num_threads; i++) {
        args = malloc(sizeof(struct thread_args));
        args->listenfd = listenfd;
        args->idx = i;
        args->ctx = ctx;

        if (pthread_create(&threads[i], NULL, handle_client, args) != 0) {
            perror("pthread_create failed");
            exit(EXIT_FAILURE);
        }
    }

    /* 메인 쓰레드 종료 대기 */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    /* SKVS 종료 */
    skvs_destroy(ctx, 1);
    close(listenfd);
    free(threads);

    printf("Server shut down successfully.\n");
    
/*---------------------------------------------------------------------------*/

    return 0;
}
