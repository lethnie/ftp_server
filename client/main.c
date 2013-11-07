#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

const int buf_size = 100;
const int ans_size = 5000;

int main(void) //int argc, char*argv[])
{
    //char* command = (char*)malloc(sizeof(char)*5);
    //char* path = (char*)malloc(sizeof(char)*buf_size);
    /*if (argc < 3)
    {
        printf("Недостаточное число аргументов\n");
        return 0;
    }
    else
    {
        command = argv[1];
        path = argv[2];
    }*/
    char* command = (char*)malloc(sizeof(char)*buf_size);

    int sockfd;
    int len;
    struct sockaddr_in address;
    int result;
    //char* buf[2];
    //buf[0] = command;
    //buf[1] = path;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = 9734;
    len = sizeof(address);
    result = connect(sockfd, (struct sockaddr*)&address, len);
    if (result == -1)
    {
        perror("client error");
        exit(1);
    }

    while (command[0] != 'q')
    {
        free(command);
        command = (char*)malloc(sizeof(char)*buf_size);
        getline(&command, &buf_size, stdin);
        write(sockfd, command, strlen(command)*sizeof(char));
        if (strcasecmp(command,"q\n"))
        {
            char* answer = (char*)malloc(sizeof(char)*ans_size);
            read(sockfd, answer, ans_size);
            printf("%s",answer);
            answer = "\n";
        }
    }
    free(command);

    close(sockfd);
    exit(0);
}
