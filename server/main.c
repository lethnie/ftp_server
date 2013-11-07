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
int server_sockfd;
//const char f_permissions[3] = {'r','w','x'};

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
    int count;
}list_answer_t;

char* get_list_answer_file(list_answer_file_t* answer);

char* get_list_answer(char* path)
{
    list_answer_t* answer = (list_answer_t*)malloc(sizeof(list_answer_t));
    answer->count = 0;
    //printf("before open\n");
    DIR* dir = opendir(path); 

    char* result = (char*)malloc(sizeof(char)*ans_size);
    int z = 0;
    for (z = 0; z < ans_size; z++)
        result[z] = '\0';
    //printf("opened\n");
    if (dir == NULL)
    {
        return "wrong path\n";
    }
    else
    {
        struct dirent* dirstr;
        //printf("before read\n");
        while ((dirstr = readdir(dir)) != NULL)
        {
            //printf("start\n");
            answer->count++;
            list_answer_file_t* files = (list_answer_file_t*)malloc(sizeof(list_answer_file_t)*answer->count);
            int i = 0;
            //printf("before cycle\n");
            for (i = 0; i < answer->count - 1; i++)
            {
                files[i] = answer->files[i];
            }
            int j = 0;
            //printf("filename\n");
            int length = strlen(dirstr->d_name);
            files[i].filename = (char*)malloc(sizeof(char)*(length + 1));

            //printf("length % i\n", length);// %i\n", length);
            for (j = 0; j < length; j++)
            {
                files[i].filename[j] = dirstr->d_name[j];
            }
            files[i].filename[j] = '\0';
            //printf("filename: %s\n", files[i].filename);
            struct stat fileinfo;
            //printf("stat\n");
            stat(files[i].filename, &fileinfo);
            files[i].number = fileinfo.st_nlink;
            files[i].filesize = fileinfo.st_size;
            files[i].group = fileinfo.st_gid;
            files[i].owner = fileinfo.st_uid;
            files[i].permissions = fileinfo.st_mode;
            files[i].time = fileinfo.st_ctim;
            answer->files = files;
            //printf("after read\n");
        }
        //printf("after read\n");
        int i = 0;
        for (i = 0; i < answer->count; i++)
        {
            //printf("list1\n");
            char* perm = get_list_answer_file(&(answer->files[i]));
            //printf("list2\n");
            //printf("perm: %s", perm);
            //printf("strcat\n");
            result = strcat(result, perm);
            //printf("list3\n");
        }
        rewinddir(dir);
        closedir(dir);
        return result;
    }
}

char* get_dir(char* d)
{
    char* res = (char*)malloc(sizeof(char)*buf_size);
    int i = 0;
    int j = 0;
    printf("d: %s", d);
    while ((i < strlen(d)) && (d[i] != '\n'))
    {
        if (d[i] == '.')
        {
            if (d[i+1] == '/')
                i += 2;
            else
            {
                if (d[i+1] == '.')
                {
                    printf('second "." j: %i\n', j);
                    if (j > 0)
                    {
                        j-=2;
                        while ((j > 0) && (res[j] != '/'))
                            j--;
                        i += 2;
                    }
                    else
                    {
                        res[j] = '.';
                        j++;
                        res[j] = '.';
                        i+=2;
                    }
                }
                else
                {
                    res[j] = d[i];
                    i++;
                    j++;
                }
            }

        }
        else
        {
            res[j] = d[i];
            i++;
            j++;
        }
    }
    for (i = j; i < buf_size; i++)
        res[i] = '\0';
    free(d);
    return res;
}

int main()
{
    //pipe(fd);
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

    while(1)
    {
        printf("server waiting\n");
        client_len = sizeof(client_address);

        client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_address, &client_len);

        struct epoll_event ev;
        ev.data.fd = client_sockfd;
        ev.events = EPOLLIN;
        epoll_ctl(efd, EPOLL_CTL_ADD, client_sockfd, &ev);

        int ready = epoll_wait(efd, evlist, EPOLL_EVENTS, -1);
        int j;
        for (j = 0; j < ready; j++)
        {
            int cfd = evlist[j].data.fd;

            if (fork() == 0)
            {
                close(server_sockfd);

                int cl = 0;
                char* dir = (char*)malloc(sizeof(char)*buf_size);
                int dir_length;
                char* current_dir = (char*)malloc(sizeof(char)*buf_size);
                int curr_length = 0;

                current_dir[0] = '\0';
                dir[0] = '.';
                dir[1] = '/';
                dir[2] = '\0';
                dir_length = 2;

                char* command = (char*)malloc(sizeof(char)*buf_size);
                char* result = (char*)malloc(sizeof(char)*ans_size);
                while (!cl)
                {

                    char** buf = (char**)malloc(sizeof(char*)*2);
                    buf[0] = (char*)malloc(sizeof(char)*5);
                    buf[1] = (char*)malloc(sizeof(char)*buf_size);

                    command = (char*)malloc(sizeof(char)*buf_size);

                    int i = 0;

                    read(cfd, command, buf_size);


                    if (command[0] == 'q')
                    {
                        cl = 1;
                        printf("Q\n");
                    }
                    else
                    {
                        cl = 0;
                        free(result);
                        free(buf[0]);
                        free(buf[1]);
                        char* result = (char*)malloc(sizeof(char)*ans_size);
                        int z;
                        for (z = 0; z < ans_size; z++)
                            result[z] = '\0';
                        buf[0] = (char*)malloc(sizeof(char)*5);
                        buf[1] = (char*)malloc(sizeof(char)*buf_size);

                        i = 0;
                        //printf("dir1: %s\n", dir);
                        while ((i < strlen(command)) && (command[i] != ' ') && (command[i] != '\n'))
                        {
                            buf[0][i] = command[i];
                            i++;
                        }
                        buf[0][i] = '\n';
                        //printf("command: %s", buf[0]);

                        while ((i < strlen(command)) && (command[i] == ' '))
                        {
                            i++;
                        }
                        if (command[i] == '\n')
                        {
                            buf[1] = "./";
                            //printf("dir2: %s\n", dir);
                        }
                        else
                        {
                            int i1 = i;
                            while ((i < strlen(command)) && (command[i] != ' ') && (command[i] != '\n'))
                            {
                                buf[1][i - i1] = command[i];
                                i++;
                            }
                            buf[1][i - i1] = '\n';
                            //printf("path: %s", buf[1]);///////////////////////

                }
                        int k = 0;
                        //printf("curr_length: %i\n", curr_length);
                        for (k = 0; k < curr_length; k++)
                            dir[k] = current_dir[k];
                        //printf("dir after current_dir: %s\n", dir);
                        k = 0;
                        while (buf[1][k] != '\n')
                        {
                            dir[curr_length + k] = buf[1][k];
                            k++;
                        }

                        dir[curr_length + k] = '\0';
                        //printf("dir after: %s\n", dir);
                        dir_length = curr_length + k - 1;

                if (strncasecmp(buf[0],"LIST\n", strlen(buf[0]) - 1) == 0)
                {
                    result = get_list_answer(dir);
                }
                else
                {
                    if (strncasecmp(buf[0],"CWD\n", 3) == 0)
                    {
                        char* tmp = (char*)malloc(sizeof(char)*buf_size);
                        if (buf[1][0] == '.')
                        {
                        int i = 0;
                        for (i = 0; i < curr_length; i++)
                        {
                            tmp[i] = current_dir[i];
                        }
                        int j = i;
                        if ((strlen(buf[1])) && (j > 0) && (tmp[j-1] != '/') && (buf[1][0] != '/'))
                        {
                            tmp[j] = '/';
                            j++;
                        }
                        i = 0;
                        //int i1 = 0;
                        //if (buf[1][1] == '/')
                         //   i1 = 2;
                        //while ((i1 < strlen(buf[1])) && (buf[1][i1] != '\n')
                        //       && (buf[1][i1] == '.') && (buf[1][i1+1] == '/'))
                        //    i1+=2;

                        while (i < strlen(buf[1]) && (buf[1][i] != '\n'))
                        {
                            tmp[j + i] = buf[1][i];
                            i++;
                        }
                        if (tmp[j+i-1] != '/')
                            tmp[j+i] = '/';
                        else
                            tmp[j+i] = '\0';
                        for (i = j + i + 1; i < buf_size; i++)
                            tmp[i] = '\0';
                        //tmp = get_dir(tmp);
                        printf("TMP: %s\n", tmp);

                        DIR* d = opendir(tmp);
                        if (d == NULL)
                            result = "Wrong path\n";
                        else
                        {
                            closedir(d);
                            for (i = 0; i < strlen(tmp); i++)
                            {
                                current_dir[i] = tmp[i];
                            }

                            current_dir[i] = '\n';
                            curr_length = i;
                            result = "Current directory was changed\n";
                        }
                        free(tmp);
                        }
                        else
                        {
                            int i = 0;
                            //printf("DIR: %s\n", buf[1]);
                            for (i = 0; i < strlen(buf[1]) - 1; i++)
                            {
                                tmp[i] = buf[1][i];
                            }
                            if (tmp[i-1] != '/')
                                tmp[i] = '/';
                            else
                                tmp[i] = '\0';
                            for (i = i + 1; i < buf_size; i++)
                                tmp[i] = '\0';
                            //tmp = get_dir(tmp);

                            printf("TMP: %s\n", tmp);

                            DIR* d = opendir(tmp);
                            if (d == NULL)
                                result = "Wrong path\n";
                            else
                            {
                                closedir(d);
                                for (i = 0; i < strlen(tmp); i++)
                                {
                                    current_dir[i] = tmp[i];
                                }

                                current_dir[i] = '\n';
                                curr_length = i;
                                result = "Current directory was changed\n";
                            }
                            free(tmp);
                        }
                    }
                    else
                    {
                        result = "wrong command\n";
                    }
                }

                write(cfd, result, strlen(result)*sizeof(char));
                result = "";
                command = "";
                buf[0]="\n";
                buf[1]="\n";

                }

           }
                close(cfd);
                exit(0);

        }
            epoll_ctl(efd, EPOLL_CTL_DEL, cfd, &evlist[j]);
            close(client_sockfd);
        }

    }

    close(server_sockfd);
}

char* get_list_answer_file(list_answer_file_t* answer)
{
    char* result = (char*)malloc(sizeof(char)*buf_size);
    result[0] = '-';
    switch (answer->permissions & S_IFMT)
    {
    case S_IFDIR: {
        result[0] = 'd';
        break;
    }
    case S_IFCHR: {
        result[0] = 'c';
        break;
    }
    case S_IFBLK: {
        result[0] = 'b';
        break;
    }
    case S_IFREG : {
        result[0] = '-';
        break;
    }
    case S_IFLNK: {
        result[0] = 'l';
        break;
    }
    case S_IFSOCK: {
        result[0] = 's';
        break;
    }
    }

    int q = 1000;
    int perm = 0;
    for (perm = 0; perm < 3; perm++)
    {
        int x = (answer->permissions/q)%10;
        if (x > 3)
        {
            result[1 + perm*3] = 'r';
            x -= 4;
        }
        else
        {
            result[1 + perm*3] = '-';
        }
        if (x % 2)
        {
            result[3 + perm*3] = 'x';
            x--;
        }
        else
        {
            result[3 + perm*3] = '-';
        }
        if (x > 0)
        {
            result[2 + perm*3] = 'w';
        }
        else
        {
            result[2 + perm*3] = '-';
        }
        q /= 10;
    }

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
    i++;
    result[i] = '\n';
    return result;
}
