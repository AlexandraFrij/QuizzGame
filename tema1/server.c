#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>

 int loged_status = 0;

int main()
{
    //comunicarea client-server
    if((access("fifo1", F_OK)) == -1) // fifo nu exista, il creez
    {
        perror("Eroare, fifo1 nu exista");
        exit(EXIT_FAILURE);

    }
    int fd_client_server;
    if((fd_client_server = open("fifo1", O_RDONLY)) == -1) // deschid fifo1, verific erorile
    {
        perror("Eroare la deschidere fifo1");
        exit(EXIT_FAILURE);
    } 
    //comunicarea server-client
    if((access("fifo2", F_OK)) == -1) // fifo nu exista, il creez
    {
        perror("Eroare, fifo2 nu exista");
        exit(EXIT_FAILURE);
    }
    int fd_server_client;
    if((fd_server_client = open("fifo2", O_WRONLY)) == -1) //deschid fifo2, verific erorile
    {
        perror("Eroare la deschidere fifo2");
        exit(EXIT_FAILURE);
    }
    // citirea unei comenzi de la client
   while(1) // atat timp cat nu s-a citit comanda quit
    {
        char cmd[100];
        if( read(fd_client_server, cmd, sizeof(cmd)) == -1)
        {
            perror("Eroare la citirea comenzii");
            exit(EXIT_FAILURE);
        }
        //printf("comanda server \n");
        //printf("%s", cmd);
        if( strstr(cmd, "logout") != NULL) // s-a citit comanda de logout
        {
            if(loged_status == 0)
            {
                if ( write( fd_server_client, "Nu exista utilizatori conectati", sizeof("Nu exista utilizatori conectati")) == -1)
                {
                    perror("Eroare la scrierea raspunsului catre client");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                if( write (fd_server_client, "Utilizatorii au fost deconectati", sizeof("Utilizatorii au fost deconectati")) == -1)
                {
                    perror("Eroare la scrierea raspunsului catre client");
                    exit(EXIT_FAILURE);
                }
                loged_status = 0;
            }
        }

        if( strstr(cmd, "quit") != NULL) // s-a citit comanda quit
            {
                if ( (write(fd_server_client, "Programul s-a incheiat", sizeof("Programul s-a incheiat") )) == -1)
                {
                    perror("Eroare la scrierea raspunsului catre client");
                    exit(EXIT_FAILURE);
                }
                break;
                
            }
            if( strstr(cmd, "login") != NULL) // s-a introdus comanda login
            {
                int fd[2], child2, rv;
                if( (pipe(fd)) == -1)
                {
                    perror("Eroare la creare pipe");
                    exit(EXIT_FAILURE);
                }
                if( (child2 = fork()) == -1)
                {
                    perror("Eroare la crearea procesului copil");
                    exit(EXIT_FAILURE);
                }
                else if(child2 == 0)
                {
                    FILE *file = fopen("username.txt", "r");
                    if(file == NULL)
                    {
                        perror("Nu s-a putut deschide fisierul");
                        exit(EXIT_FAILURE);
                    }
                    char name[50];
                    int find = 0;
                    while( fgets(name, sizeof(name), file)  )
                    {
                        if( strstr(cmd, name) != NULL)
                        {
                            if( write(fd[1], "Utilizatorul a fost logat", sizeof("Utilizatorul a fost logat")) == -1)
                        {
                            perror("Eroare la scriere in procesul copil");
                            exit(EXIT_FAILURE);
                        }
                        loged_status = 1;
                        find = 1;
                        break;
                        }
                    }
                    if( find == 0)
                        if( write(fd[1], "Utilizatorul nu exista, incercati din nou", sizeof("Utilizatorul nu exista, incercati din nou")) == -1)
                    {
                        perror("Eroare la scriere in procesul copil");
                        exit(EXIT_FAILURE);
                    }
                    exit(2);
                    close(fd[0]);
                    close(fd[1]);
                }
                else
                {
                    char m[100];
                    close(fd[1]);
                    wait(&rv);
                    if( read(fd[0], m, sizeof(m)) == -1)
                    {
                        perror("Eroare la citire in procesul parinte");
                        exit(EXIT_FAILURE);
                    }
                    if(WEXITSTATUS (rv) == 2)
                    loged_status = 1;
                    if( write(fd_server_client, m, sizeof(m)) == -1)
                    {
                        perror("Eroare la scrierea catre client");
                        exit(EXIT_FAILURE);
                    }
                    close(fd[0]);
                }
            }
       /* if( strstr( cmd, "get-proc-info") != NULL)
        {
            int sockp[2], child1;
            if( socketpair(AF_UNIX, SOCK_STREAM, 0, sockp) == -1)
            {
                perror("Eroare la crearea socket");
                exit(EXIT_FAILURE);
            }
            if( ( child1 = fork()) == -1 )
            {
                perror("Eroare la crearea procesului copil");
                exit(EXIT_FAILURE);
            }
           else if( child1 == 0)
            {
                close(sockp[0]);
                char *p, fisier[300];
                p =  strtok( cmd, " ");
                while(p != NULL)
                p = strtok(NULL, "\n");
                strcat(fisier, "/proc/");
                strcat(fisier, p);
                strcat(fisier, "/status");
                FILE *fd = fopen(fisier, "r");
                if(fd == NULL)
                {
                    if(write( sockp[1], "Procesul nu exista", sizeof("Procesul nu exista")) == -1)
                    {
                        perror("Eroare la scriere in procesul copil");
                        exit(EXIT_FAILURE);
                    }
                }
                else
                {
                    char line[500];
                    while( fgets(line, sizeof(line), fd))
                {
                    if(strstr(line, "Name") != 0 || strstr(line, "State") != 0 || strstr(line, "Ppid") != 0 || strstr(line, "Uid") != 0 || strstr(line, "Vmsize") != 0)
                    if( write( sockp[1], line, sizeof(line)) == -1)
                    {
                        perror("Eroare la scriere in procesul copil");
                        exit(EXIT_FAILURE);
                    }
                }
                }
                
                close(sockp[1]);
                close(sockp[0]);

            }
            else
            {
                close(sockp[1]);
                char m[1000];
                if(read (sockp[0], m, sizeof(m)) == -1)
                {
                    perror("Eroare la citire in procesul parinte");
                    exit(EXIT_FAILURE);
                }
                if( write( fd_server_client, m, sizeof(m)) == -1)
                {
                    perror("Eroare la scrierea raspunsului catre client");
                    exit(EXIT_FAILURE);
                }
                close(sockp[0]);
                close(sockp[1]);
                    
            }
        }*/

    } 
    close(fd_server_client);
    close(fd_client_server);

}