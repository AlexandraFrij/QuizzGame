#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

int main()
{
     
    //comunicarea client-server
    if((access("fifo1", F_OK)) == -1) // fifo nu exista, il creez
    {
       if((mknod("fifo1", S_IFIFO | 0666, 0)) == -1) //verific daca apar erori la creearea fifo
        {
            perror("Eroare la creare fifo1");
            exit(EXIT_FAILURE);
        }
    }
    int fd_client_server;
    if((fd_client_server = open("fifo1", O_WRONLY ))== -1) // deschid fifo1, verific erorile
    {
        perror("Eroare la deschidere fifo1");
        exit(EXIT_FAILURE);
    } 
    // comunicarea server-client
    if((access("fifo2", F_OK))== -1) // fifo nu exista, il creez
    {
       if((mknod("fifo2", S_IFIFO | 0666, 0)) == -1) //verific daca apar erori la creearea fifo
        {
            perror("Eroare la crearea fifo2");
            exit(EXIT_FAILURE);
        }
    } 
    int fd_server_client;
    if((fd_server_client = open("fifo2", O_RDONLY)) == -1) //deschid fifo2, verific erorile
    {
        perror("Eroare la deschidere fifo2");
        exit(EXIT_FAILURE);
    }
    char comanda[100];
    while(1) // cat timp nu s-a introdus comanda quit
    {
        while(read (0, comanda, sizeof(comanda)) == -1)
        {
            perror("Eroare la citirea comezii");
            exit(EXIT_FAILURE);
        }
        //printf("%s", "comanda este");
        //printf("%s", comanda);
        if((write(fd_client_server, comanda, sizeof(comanda))) == -1 )// scriu comanda pentru server si verific erorile 
        {
            perror("Eroare la scrierea comenzii");
            exit(EXIT_FAILURE);
        }
        char raspuns[1024];
        if((read( fd_server_client, raspuns, sizeof(raspuns)))== -1) // citesc raspunsul de la server si verific erorile
        {
            perror("Eroare la primirea raspunsului");
            exit(EXIT_FAILURE);
        }
        printf("%s\n", raspuns); // scriu raspunsul pe ecran
        if((strstr(comanda, "quit")) != NULL) // daca comanda = quit, notquit = 0 ca bucla sa se poata incheia
        {
            break;
        }  
    } 

    close(fd_server_client); // inchid descriptorii la finalul programului
    close(fd_client_server);
}