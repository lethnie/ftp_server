#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

const int EPOLL_SIZE = 10;
const int EPOLL_EVENTS = 10;
const int buf_size = 100;

typedef struct list_answer_file_t
{
    mode_t permissions;
    int number;
    int owner;
    int group;
    int filesize;
    struct timespec time;
    char* filename;
}list_answer_file_t;

typedef struct list_answer_t
{
    list_answer_file_t* files;
    //char** files;
    int count;
}list_answer_t;

char* get_list_answer_file(list_answer_file_t* answer);

list_answer_t* get_list_answer(char* path)
{
    list_answer_t* answer = (list_answer_t*)malloc(sizeof(list_answer_t));
    printf("%s\n", path);
    DIR* dir = opendir(path);
    if (dir == NULL)
        return NULL; ///////////////нет такой папки
    else
    {
        struct dirent* dirstr;
        while ((dirstr = readdir(dir)) != NULL)
        {
            answer->count++;
            list_answer_file_t* files = (list_answer_file_t*)malloc(sizeof(list_answer_file_t)*answer->count);
            int i = 0;
            for (i = 0; i < answer->count - 1; i++)
            {
                files[i] = answer->files[i];
            }
            files[i].filename = dirstr->d_name;
            struct stat fileinfo;
            stat(files[i].filename, &fileinfo);
            files[i].number = fileinfo.st_nlink;
            files[i].filesize = fileinfo.st_size;
            files[i].group = fileinfo.st_gid;
            files[i].owner = fileinfo.st_uid;
            files[i].permissions = fileinfo.st_mode;
            files[i].time = fileinfo.st_ctim;
            answer->files = files;
        }
        return answer;
    }
}

typedef struct procpool_t
{
    pid_t* pid;
    int size;
    int efd;
    struct epoll_event* evlist;//[EPOLL_EVENTS];
    int busy;
}procpool_t;

procpool_t* ppool;

void procpool()
{
    while (1)
    {
        int ready = epoll_wait(ppool->efd, ppool->evlist, EPOLL_EVENTS, -1);
        if (ready > 0)
        {
            struct epoll_event ev = ppool->evlist[0];
            epoll_ctl(ppool->efd, EPOLL_CTL_DEL, ppool->evlist[0].data.fd, &ev);
            char* buf[2];
            buf[0] = (char*)malloc(sizeof(char)*5);
            buf[1] = (char*)malloc(sizeof(char)*buf_size);
            read(ev.data.fd, buf[0], sizeof(buf[0]));
            read(ev.data.fd, buf[1], sizeof(buf[1]));
            close(ev.data.fd);
            free(buf[0]);
            free(buf[1]);
        }
    }
}

procpool_t* procpool_create(int count)
{
    procpool_t *pool = (procpool_t*)malloc(sizeof(procpool_t));
    pool->pid = (pid_t*)malloc(sizeof(pid_t)*count);
    pool->size = 0;
    pool->efd = epoll_create(EPOLL_SIZE);
    pool->busy = 0;
    pool->evlist = (struct epoll_event*)malloc(sizeof(struct epoll_event)*EPOLL_EVENTS);
    int i = 0;
    for (i = 0; i < count; i++)
    {
        pid_t pid = fork();
        if (pid != 0)
        {
            pool->pid[i] = pid;
        }
        else
        {
            procpool();
        }
        pool->size++;
    }
    return pool;
}

int main()
{
    int server_sockfd, client_sockfd;
    int server_len, client_len;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    server_sockfd = socket(AF_INET,SOCK_STREAM,0);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = 9734;
    server_len = sizeof(server_address);
    //ppool = procpool_create(5);
    bind(server_sockfd, (struct sockaddr*)&server_address, server_len);
    listen(server_sockfd,5);
    signal(SIGCHLD,SIG_IGN);
    int efd = epoll_create(EPOLL_SIZE);
    struct epoll_event evlist[EPOLL_EVENTS];
    while(1)
    {
        char* buf[2];
        buf[0] = (char*)malloc(sizeof(char)*5);
        buf[1] = (char*)malloc(sizeof(char)*buf_size);
        printf("server waiting\n");
        client_len = sizeof(client_address);
        client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_address, &client_len);
        /*struct epoll_event ev;
        ev.data.fd = client_sockfd;
        ev.events = EPOLLIN;
        epoll_ctl(efd, EPOLL_CTL_ADD, client_sockfd, &ev);
        int ready = epoll_wait(efd, evlist, EPOLL_EVENTS, -1);
        int j;
        for (j = 0; j < ready; j++)
        {
            read(evlist[j].data.fd, buf[0], sizeof(buf[0]));
            read(evlist[j].data.fd, buf[1], sizeof(buf[1]));
            //write(evlist[j].data.fd, buf[0], sizeof(buf[0]));
        }
        epoll_ctl(efd, EPOLL_CTL_DEL, client_sockfd, &ev);
        close(client_sockfd);*/

        if (fork() == 0)
        {
            read(client_sockfd, buf[0], sizeof(buf[0]));
            read(client_sockfd, buf[1], buf_size);
            printf("%d\n", strlen(buf[0]));
            if (buf[0][0] == 'L')
            {
                printf("%d\n", sizeof(buf[1]));
                printf("list %s\n", buf[1]);
                list_answer_t* ans = get_list_answer(buf[1]);
                int i;
                for (i = 0; i < ans->count; i++)
                {
                    char* perm = get_list_answer_file(&(ans->files[i]));
                    printf("%s", perm);
                }
            }
            else
            {
                if (buf[0][0] == 'C')
                {
                    printf("cwd\n");
                }
            }

            write(client_sockfd, buf[0], sizeof(buf[0]));
            close(client_sockfd);
            exit(0);
        }
        else
        {
            close(client_sockfd);
        }
    }
}

char* get_list_answer_file(list_answer_file_t* answer)
{
    char* result = (char*)malloc(sizeof(char)*buf_size);////???
    if (S_IFDIR & answer->permissions) {
        result[0] = 'd'; }
    else
    {
        if (S_IFREG & answer->permissions) {
            result[0] = '-'; }
        else
        {
            if (S_IFCHR & answer->permissions) {
                result[0] = 'c'; }
            else
            {
                if(S_IFIFO & answer->permissions) {
                    result[0] = 'p'; }
                else
                {
                    if (S_IFSOCK & answer->permissions) {
                        result[0] = 's'; }
                    else
                    {
                        if (S_IFBLK & answer->permissions) {
                            result[0] = 'b'; }
                        else
                        {
                            if (S_IFLNK & answer->permissions) {
                                result[0] = 'l'; }
                        }
                    }
                }
            }
        }
    }

    if (S_IRUSR & answer->permissions)
        result[1] = 'r';
    else
        result[1] = '-';
    if (S_IWUSR & answer->permissions)
        result[2] = 'w';
    else
        result[2] = '-';
    if (S_IXUSR & answer->permissions)
        result[3] = 'x';
    else
        result[3] = '-';
    if (S_IRGRP & answer->permissions)
        result[4] = 'r';
    else
        result[4] = '-';
    if (S_IWGRP & answer->permissions)
        result[5] = 'w';
    else
        result[5] = '-';
    if (S_IXGRP & answer->permissions)
        result[6] = 'x';
    else
        result[6] = '-';
    if (S_IROTH & answer->permissions)
        result[7] = 'r';
    else
        result[7] = '-';
    if (S_IWOTH & answer->permissions)
        result[8] = 'w';
    else
        result[8] = '-';
    if (S_IXOTH & answer->permissions)
        result[9] = 'x';
    else
        result[9] = '-';

    result[10] = ' ';
    char* l = (char*)malloc(sizeof(char)*3);
    sprintf(l,"%i\n",answer->number);
    int i = 0;
    for (i = 0; i < strlen(l); i++)
        result[11+i] = l[i];

    i += 10;
    result[i] = ' ';
    i++;

    free(l);
    l = (char*)malloc(sizeof(char)*5);
    sprintf(l,"%i\n",answer->owner);
    int j = 0;
    for (j = 0; j < strlen(l); j++)
        result[j+i] = l[j];

    i += j - 1;
    result[i] = ' ';
    i++;

    free(l);
    l = (char*)malloc(sizeof(char)*5);
    sprintf(l,"%i\n",answer->group);
    j = 0;
    for (j = 0; j < strlen(l); j++)
        result[j+i] = l[j];

    i += j - 1;
    result[i] = ' ';
    i++;

    free(l);
    l = (char*)malloc(sizeof(char)*10);
    sprintf(l,"%i\n",answer->filesize);
    j = 0;
    for (j = 0; j < strlen(l); j++)
        result[j+i] = l[j];

    i += j - 1;
    result[i] = ' ';
    while (i <30)
    {
        i++;
        result[i] = ' ';
    }

    free(l);
    l = (char*)malloc(sizeof(char)*40);
    struct tm* t = localtime(&answer->time.tv_sec);
    //strftime(l, sizeof(l), "%m-%d %H:%M:%S\n", t);
    sprintf(l, "%i-%i %i:%i:%i\n", t->tm_mon, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    j = 0;
    for (j = 0; j < strlen(l); j++)
        result[j+i] = l[j];

    i += j - 1;
    result[i] = ' ';
    while (i < 45)
    {
        i++;
        result[i] = ' ';
    }
    free(l);

    j = 0;
    for (j = 0; j < strlen(answer->filename); j++)
        result[j+i] = answer->filename[j];

    i += j - 1;
    //result[i] = ' ';
    i++;
    result[i] = '\n';

    return result;
}
