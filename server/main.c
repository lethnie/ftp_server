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
#include <sys/ipc.h>
#include <sys/shm.h>

const int EPOLL_SIZE = 10;
const int EPOLL_EVENTS = 10;
const int buf_size = 100;
const int ans_size = 5000;
//const int shm_buf_size = 4096;
int fd[2];
//int shmid;
//void* shm;

typedef struct task_queue_elem_t
{
    int fd;
    struct task_queue_elem_t* next;
} task_queue_elem_t;

typedef struct tasks_queue_t
{
    task_queue_elem_t* head;
    task_queue_elem_t* tail;
}tasks_queue_t;

tasks_queue_t* tasks;

void add_task(tasks_queue_t *queue, int fd)
{
    task_queue_elem_t* elem = (task_queue_elem_t*)malloc(sizeof(task_queue_elem_t));
    elem->next = NULL;
    elem->fd = fd;
    if (queue->head == NULL)
    {
        queue->head = elem;
        queue->tail = elem;
    }
    else
    {
        (*(queue->tail)).next = elem;
        queue->tail = queue->tail->next;
    }
    //tasks_count++;
}

int delete_task(tasks_queue_t *queue)
{
    int sfd = queue->head->fd;
    task_queue_elem_t* head = queue->head;
    free(head);
    queue->head = queue->head->next;
    return sfd;
    //ppool->tasks_count--;
}

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
    //answer->count = 0;
    DIR* dir = opendir(path);
    if (dir == NULL)
    {
        return NULL;
    }
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
        //closedir(dir);
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
    //tasks_queue_t* tasks;
    //int tasks_count;
}procpool_t;

procpool_t* ppool;
int server_sockfd;

void procpool()
{
    close(server_sockfd);
    while (1)
    {        
        int client_sfd;
        read(fd[0], &client_sfd, sizeof(int));
        //tasks_queue_t* t_queue = (tasks_queue_t*)shm;
        //client_sfd = delete_task(tasks);

        printf("client_sockfd %i\n", client_sfd);
        char** buf = (char**)malloc(sizeof(char*)*2);
        char* command = (char*)malloc(sizeof(char)*buf_size);
        read(client_sfd, command, buf_size);

        buf[0] = (char*)malloc(sizeof(char)*5);
        buf[1] = (char*)malloc(sizeof(char)*buf_size);

        int i = 0;
        while ((i < strlen(command)) && (command[i] != ' '))
        {
            buf[0][i] = command[i];
            i++;
        }
        while ((i < strlen(command)) && (command[i] == ' '))
        {
            i++;
        }
        int i1 = i;
        while ((i < strlen(command)))
        {
            buf[1][i - i1] = command[i];
            i++;
        }
        printf("%s\n", buf[1]);
        write(client_sfd, buf[1], strlen(buf[1])*sizeof(char));

        close(client_sfd);

    }
    //shmctl(shmid, IPC_RMID, NULL);
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
            close(fd[1]);
            procpool();
        }
        pool->size++;
    }
    return pool;
}

int main()
{
    pipe(fd);
    int client_sockfd;
    int server_len, client_len;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    server_sockfd = socket(AF_INET,SOCK_STREAM,0);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = 9734;
    server_len = sizeof(server_address);
    bind(server_sockfd, (struct sockaddr*)&server_address, server_len);
    listen(server_sockfd,5);
    signal(SIGCHLD,SIG_IGN);
    int efd = epoll_create(EPOLL_SIZE);
    struct epoll_event evlist[EPOLL_EVENTS];
    tasks = (tasks_queue_t*)malloc(sizeof(tasks_queue_t));
    tasks->head = NULL;
    tasks->tail = NULL;

    /*key_t key = 123456;

    shmid = shmget(key, shm_buf_size, IPC_CREAT|0666);
    if (shmid == -1)
    {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    shm = shmat(shmid,NULL,0);
    if (shm == (void*)-1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    tasks_queue_t* t_queue = (tasks_queue_t*)shm;
    t_queue->head = NULL;
    t_queue->tail = NULL;*/
    /*struct epoll_event ev;
    ev.data.fd = server_sockfd;
    ev.events = EPOLLIN;
    epoll_ctl(efd, EPOLL_CTL_ADD, server_sockfd, &ev);*/
    while(1)
    {
        printf("server waiting\n");
        client_len = sizeof(client_address);
        client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_address, &client_len);
        struct epoll_event ev;
        ev.data.fd = client_sockfd;
        ev.events = EPOLLIN;
        epoll_ctl(efd, EPOLL_CTL_ADD, client_sockfd, &ev);
        /*if (ppool == NULL)
        {
            ppool = procpool_create(5);
            close(fd[0]);
        }*/
        int ready = epoll_wait(efd, evlist, EPOLL_EVENTS, -1);
        int j;
        for (j = 0; j < ready; j++)
        {
            int cfd = evlist[j].data.fd;
            add_task(tasks, cfd);
            //write(fd[1],&cfd,sizeof(int));
            if (fork() == 0)
            {

            int client_sfd = delete_task(tasks);
            //while (command[0] != 'q')
            //{
            char** buf = (char**)malloc(sizeof(char*)*2);
            char* command = (char*)malloc(sizeof(char)*buf_size);
            read(client_sfd, command, buf_size);

            buf[0] = (char*)malloc(sizeof(char)*5);
            buf[1] = (char*)malloc(sizeof(char)*buf_size);

            int i = 0;
            while ((i < strlen(command)) && (command[i] != ' ') && (command[i] != '\n'))
            {
                buf[0][i] = command[i];
                i++;
            }
            while ((i < strlen(command)) && (command[i] == ' '))
            {
                i++;
            }
            int i1 = i;
            while ((i < strlen(command)) && (command[i] != ' ') && (command[i] != '\n'))
            {
                buf[1][i - i1] = command[i];
                i++;
            }
            char* result = (char*)malloc(sizeof(char)*ans_size);
            if (strcasecmp(buf[0],"LIST") == 0)
            {
                list_answer_t* ans = get_list_answer(buf[1]);
                if (ans == NULL)
                {
                    result = "wrong path\n";
                }
                else
                {
                    int i;
                    for (i = 0; i < ans->count; i++)
                    {
                        char* perm = get_list_answer_file(&(ans->files[i]));
                        //printf("%s", perm);
                        result = strcat(result, perm);
                    }
                }

            }
            else
            {
                if (strcasecmp(buf[0],"CWD") == 0)
                {
                    printf("cwd\n");
                }
                else
                {
                    result = "wrong command\n";
                }
            }
            //free(command);
           // free(buf[0]);
            //free(buf[1]);

            write(client_sfd, result, strlen(result)*sizeof(char));
            //free(result);
            //}
            close(server_sockfd);
            close(client_sfd);

            exit(0);
            }

            epoll_ctl(efd, EPOLL_CTL_DEL, cfd, &evlist[j]);
            close(cfd);
        }

    }
    close(server_sockfd);
    //shmdt(shm);
    //shmctl(shmid, IPC_RMID, NULL);
}

char* get_list_answer_file(list_answer_file_t* answer)
{
    char* result = (char*)malloc(sizeof(char)*buf_size);

    if (S_IFREG & answer->permissions) {
        result[0] = '-'; }
    else
    {
        if (S_IFDIR & answer->permissions) {
            result[0] = 'd'; }
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
    //struct passwd* userinfo = getpwuid(answer->owner);
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
    //struct group* groupinfo = getgrgid(answer->group);
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
