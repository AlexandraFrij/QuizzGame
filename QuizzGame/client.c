#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>

/* codul de eroare returnat de anumite apeluri */
extern int errno;
/* portul de conectare la server*/
int port;
int quit = 0;
int time_limit = 10;
int main (int argc, char *argv[])
{
  int sd;			// descriptorul de socket
  struct sockaddr_in server;	// structura folosita pentru conectare 

  /* exista toate argumentele in linia de comanda? */
  if (argc != 3)
    {
      printf ("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
      return -1;
    }

  port = atoi (argv[2]);
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("Eroare la socket().\n");
      return errno;
    }

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(argv[1]);
  server.sin_port = htons (port);
  if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
      perror ("[client]Eroare la connect().\n");
      return errno;
    }
  // mesaj dupa conectare
  char msg[100];
  if(read(sd, msg, sizeof(msg)) == -1)
  {
      perror("Eroare la citirea de la server[cerere nickname]\n");
      return errno;
  }
  else 
  {
    printf("%s\n",msg);
  }

  // introducere nickname
  char nickname[100];
  scanf("%s", nickname);
  if(write(sd, nickname, sizeof(nickname)) == -1)
  {
      perror("Eroare la scrierea nickname catre server\n");
      return errno;
  }
  printf("In continuare vei primi o serie de intrebari si posibilele raspunsuri.\n");
  int questions_nb;
  if(read(sd, &questions_nb, sizeof(questions_nb)) == -1)
  {
    perror("Eroare la citirea numarului de intrebari\n");
    return errno;
  }
  printf("Numarul intrebarilor la care trebuie sa raspunzi este: %d\n", questions_nb);
  printf("Introdu numarul raspunsului ales sau quit daca vrei sa parasesti jocul\n");
  printf("Ai 10 secunde pentru a raspunde fiecarei intrebari\n");
  char intrebare[100];
  int answers_nb;
  char raspuns[100];
  char rsp[100];
  int nr;
  for(int i = 0; i < questions_nb; i++)
  {
    if(read(sd, &nr, sizeof(int)) == -1)
    {
      perror("Eroare la citirea numarului intrebarii\n");
      return errno;
    }
    else printf("Intrebarea %d\n", nr);
    bzero(&intrebare, sizeof(intrebare));
    if(read(sd, intrebare, sizeof(intrebare)) == -1)
    {
      perror("Eroare la citirea intrebarii\n");
      return errno;
    }
    else printf("%s\n", intrebare);
    if(read(sd, &answers_nb, sizeof(answers_nb)) == -1)
    {
      perror("Eroare la citirea numarului de raspunsuri\n");
      return errno;
    }
    for(int j = 0; j < answers_nb; j++)
    {
      if(read(sd, raspuns, sizeof(raspuns)) == -1)
      {
        perror("Eroare la citirea raspunsurilor\n");
        return errno;
      }
      else printf("%d. %s\n", j+1, raspuns);
    } 
    printf("Introdu un raspuns: \n");
    scanf("%s",rsp);
    if(write(sd, rsp, sizeof(rsp)) == -1)
    {
      perror("Eroare la scrierea raspunsului catre server\n");
      return errno;
    }
    if(strstr(rsp, "quit") != NULL)
    {
      printf("%s\n", "Te-ai deconectat!");
      quit = 1;
      break;
    }
    if(read(sd, msg, sizeof(msg)) == -1)
    {
      perror("Eroare la citirea mesajului de la server\n");
      return errno;
    }
    else printf("%s\n", msg);
  }
  if(quit == 0)
  {
    if(read(sd, msg, sizeof(msg)) == -1)
    {
      perror("Eroare la citirea mesajului de la server\n");
      return errno;
    }
  else printf("%s\n", msg);
  printf("Asteptam ca toti clientii sa termine....\n");  
  //clientul primeste punctajul
  
  int scor, rank;
  if(read(sd, &scor, sizeof(scor)) == -1)
  {
    perror("Eroare la citirea mesajului de la server[punctajul]\n");
    return errno;
  }
  else
  {
    printf("Punctele tale sunt: \n");
    printf("%d\n", scor);
  } 
  //clientul primeste locul pe care s-a clasat
  
  if(read(sd, &rank, sizeof(rank)) == -1)
  {
    perror("Eroare la citirea mesajului de la server[rank]\n");
    return errno;
  }
  else
  {
    printf("Locul pe care te-ai clasat este: \n");
    printf("%d\n", rank);
  } 
  //clientul primeste numele castigatorului
 
  if(read(sd, msg, sizeof(msg)) == -1)
  {
    perror("Eroare la citirea mesajului de la server[castigatorul]\n");
    return errno;
  }
  else 
  { 
    printf("Castigatorul acestei partide de joc este: \n");
    printf("%s\n", msg);
  }
  }
  //inchidem conexiunea, am terminat 
  close (sd);
}