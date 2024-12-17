/*---------------------------------------------------------------------------*/
/* client.c                                                                  */
/* Author: Junghan Yoon, KyoungSoo Park                                      */
/* Modified by: (Your Name)                                                  */
/*---------------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <getopt.h>
#include <errno.h>
#include "common.h"
/*---------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    char *ip = DEFAULT_LOOPBACK_IP;
    int port = DEFAULT_PORT;
    int interactive = 0; /* Default is non-interactive mode */
    int opt;

/*---------------------------------------------------------------------------*/
    /* free to declare any variables */
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
/*---------------------------------------------------------------------------*/

    /* parse command line options */
    while ((opt = getopt(argc, argv, "i:p:th")) != -1)
    {
        switch (opt)
        {
        case 'i':
            ip = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            if (port <= 1024 || port >= 65536)
            {
                perror("Invalid port number");
                exit(EXIT_FAILURE);
            }
            break;
        case 't':
            interactive = 1;
            break;
        case 'h':
        default:
            printf("Usage: %s [-i server_ip_or_domain (%s)] "
                   "[-p port (%d)] [-t]\n",
                   argv[0],
                   DEFAULT_LOOPBACK_IP, 
                   DEFAULT_PORT);
            exit(EXIT_FAILURE);
        }
    }

/*---------------------------------------------------------------------------*/
    /* edit here */

    /* 서버에 연결 설정 */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid IP address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to %s:%d\n", ip, port);

    /* 인터랙티브 모드 */
    if (interactive)
    {
        while (1)
        {
            printf("Enter command: ");
            if (fgets(buffer, sizeof(buffer), stdin) == NULL)
            {
                printf("Input terminated. Closing connection.\n");
                break;
            }

            /* 입력된 명령 전송 */
            if (send(sockfd, buffer, strlen(buffer), 0) < 0)
            {
                perror("send failed");
                break;
            }

            /* 서버 응답 수신 */
            bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received > 0)
            {
                buffer[bytes_received] = '\0'; // Null-terminate buffer
                printf("Server reply: %s", buffer);
            }
            else if (bytes_received == 0)
            {
                printf("Server closed the connection.\n");
                break;
            }
            else
            {
                perror("recv failed");
                break;
            }
        }
    }
    else
    {
        /* 비-인터랙티브 모드: 명령어 파일 입력 */
        while (fgets(buffer, sizeof(buffer), stdin) != NULL)
        {
            /* 명령 전송 */
            if (send(sockfd, buffer, strlen(buffer), 0) < 0)
            {
                perror("send failed");
                break;
            }

            /* 서버 응답 수신 */
            bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received > 0)
            {
                buffer[bytes_received] = '\0';
                printf("%s", buffer);
            }
            else if (bytes_received == 0)
            {
                printf("Server closed the connection.\n");
                break;
            }
            else
            {
                perror("recv failed");
                break;
            }
        }
    }

    close(sockfd);
    printf("Connection closed.\n");

/*---------------------------------------------------------------------------*/

    return 0;
}
