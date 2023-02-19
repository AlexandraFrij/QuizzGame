#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sqlite3.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>

#define PORT 3485

pthread_mutex_t mutex;
pthread_cond_t cv;

struct sockaddr_in server;	// structura folosita de server
struct sockaddr_in from;	
int nr;		                //mesajul primit de trimis la client 
int sd;		               //descriptorul de socket 
int pid;
pthread_t th[10000];   //Identificatorii thread-urilor care se vor crea

struct jucatori
{
  char nickname[100];      // nickname-ul introdus de client
  int points;             // punctele acumulate
  int rank;              // locul pe care se afla fiecare jucator
  int giveUp;           // 1 daca a ales sa paraseasca jocul
  int done;            // 1 daca clientul a terminat/ parasit jocul
  int session;        // numarul sesiuni in care se afla clientul 
} players[1000];

int players_number = 0;
int clients = 0;
int session = 0;
int timer_length = 20 ;
bool time_running = false;  //true cand s-a conectat primul client intr-o sesiune
int time_limit = 7;        //numarul de secunde in care un client poate raspunde la o intrebare

// initializare jucatori
void initializare(int j)
{
  strcpy(players[j].nickname,"jucator");
  players[j].points = 0;
  players[j].rank = 1;
  players[j].giveUp = 0;
  players[j].done = 0;
  players[j].session = session;
}

// clasametul
void clasament(int player)
{
  for (int i = 0; i < players_number; i++)
      if(players[i].giveUp == 0)
      if(players[player].session == players[i].session && player != i)
      {
        if (players[player].points < players[i].points)
        players[player].rank++;
      }
}

//numele castigatorului
void winner(int player, char c[100])
{
  for (int i = 0; i < players_number; i++)
  {
    if(players[player].session == players[i].session)
    if(players[i].giveUp == 0 && players[i].rank == 1)
    {
      strcpy(c, players[i].nickname);
      break;
    }
  }
}

//structura pentru o sesiune ce retine nr de thread-uri din fiecare sesiune
struct session
{
  int threads_num;
} s[10000];

//initilializare sesiune
void session_init()
{
  for (int i = 0; i < 10000; i++)
  {
    s[i].threads_num = 0;
  }
}

//functiile pentru baza de date
sqlite3 *db;
sqlite3_stmt *stmt;
int questions_nb;              // nr de intrebari
int id[100];                  //nr fiecarei intrebari
int corect;                  //nr raspunsului corect al fiecarei intrebari
int punctaj[100];           //punctajul fiecarei intrebari
int answers_nb;            // nr de raspunsuri pentru fiecare intrebare
char intrebare[100][100]; //enunturile intrebarilor
char raspuns[100][100];  //raspunsurile pentru fiecare intrebare

// selectare intrebari
void select_question()
{
  if(sqlite3_open("quizz.db", &db) != SQLITE_OK)
  {
    perror("Eroare la deschiderea bazei de date\n");
  }
  const char *command = "select id_intrebare, enunt, punctaj from intrebari";
  if(sqlite3_prepare_v2(db, command, -1, &stmt, NULL) != SQLITE_OK)
  {
    perror("Eroare la sqlite3_prepare in select_question\n");
  }
  int nr = 0;
  while(sqlite3_step(stmt)!= SQLITE_DONE)
  {
   id[nr] = sqlite3_column_int(stmt, 0);
   strcpy(intrebare[nr], sqlite3_column_text(stmt, 1));
   punctaj[nr] = sqlite3_column_int(stmt, 2);
   nr++;
  }
  questions_nb = nr;
  sqlite3_close(db);
}
// selectare raspunsuri
void select_answers( int nr)
{
  if (sqlite3_open("quizz.db", &db) != SQLITE_OK)
  {
    perror("Eroare la deschiderea bazei de date\n");
  }
  const char *command = "select enunt from raspunsuri where id_intrebare = ?";
  if (sqlite3_prepare_v2(db, command, -1, &stmt, NULL) != SQLITE_OK)
  {
    perror("Eroare la sqlite3_prepare in select_answers\n");
  }
  sqlite3_bind_int(stmt, 1 , nr);
  int i = 0;
  while (sqlite3_step(stmt) != SQLITE_DONE)
  {
    strcpy(raspuns[i],sqlite3_column_text(stmt, 0));
    i++;
  }
  answers_nb = i;
  sqlite3_close(db);
}

// selectare raspuns corect
int select_correct_answer(int nr)
{
  int corect;
  if (sqlite3_open("quizz.db", &db) != SQLITE_OK)
  {
    perror("Eroare la deschiderea bazei de date\n");
  }
  const char *command = "select id_raspuns from raspunsuri_corecte where id_intrebare = ?";
  if (sqlite3_prepare_v2(db, command, -1, &stmt, NULL) != SQLITE_OK)
  {
    perror("Eroare la sqlite3_prepare in select_answers\n");
  }
  sqlite3_bind_int(stmt, 1 , nr);
  while(sqlite3_step(stmt)!= SQLITE_DONE)
  {
    corect = sqlite3_column_int(stmt, 0);
  }
  return(corect);
  sqlite3_close(db);
}
typedef struct thData
{
  int idThread;  // id-ul thread-ului tinut in evidenta de acest program
  int cl;       // descriptorul intors de accept
} thData;

static void *treat(void *);
void init(void *);
void quizz(void *);
void send_clasament(void *);
void send_winner(void *);
void make_clasament(void *);

// functia pentru a termina o sesiune 
void end_session()
{
  time_running = false;
  clients = 0;
  
}
int main()
{ 
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&cv, NULL);
  session_init();
  int i = 0;
  if((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[server]Eroare la socket().\n");
      return errno;
    }
  int on=1;
  setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
  bzero (&server, sizeof (server));
  bzero (&from, sizeof (from));
  server.sin_family = AF_INET;	
  server.sin_addr.s_addr = htonl (INADDR_ANY);
  server.sin_port = htons (PORT);

  if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
      perror ("[server]Eroare la bind().\n");
      return errno;
    }
  if (listen (sd, 2) == -1)
    {
      perror ("[server]Eroare la listen().\n");
      return errno;
    }
  time_t start_time = time(NULL);
  while (1)
    {
      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(sd, &rfds);
      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = 0;
      int res = select(sd + 1, &rfds, NULL, NULL, &timeout ); 
      if(res == -1)
      {
        perror("[server] eroare la select()\n");
      }
      if(res == 0)
      {
        time_t end_time = time(NULL);
        double elapsed_time = difftime(end_time, start_time);
        if(elapsed_time >= timer_length)
        {
          session++;
          start_time = time(NULL);
        }
        continue;
      }
      int client;
      thData * td; //parametru functia executata de thread     
      int length = sizeof (from);
      printf ("[server]Asteptam la portul %d...\n",PORT);
      fflush (stdout);
      if ( (client = accept (sd, (struct sockaddr *) &from, &length)) < 0)
      {
        perror ("[server]Eroare la accept().\n");
        continue;
      }
      players_number ++;
      td=(struct thData*)malloc(sizeof(struct thData));	
      td->idThread=i++;
      td->cl=client;
      if(pthread_create(&th[i], NULL, &treat, td)  == -1)
      {
        perror("[server]Eroare la pthread_create()\n");
      }	   
      else
      {
        s[session].threads_num++;
      }
      
	}//while    
  return 0;
};

static void *treat(void * arg)
{		
		struct thData tdL; 
		tdL= *((struct thData*)arg);	
		printf ("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
		fflush (stdout);		 
		pthread_detach(pthread_self());		
		init((struct thData*)arg);
    quizz((struct thData*)arg);
    if(players[tdL.idThread].giveUp != 1)
    {
      printf("Thread-uri in sesiune %d\n", s[players[tdL.idThread].session].threads_num);
      pthread_mutex_lock(&mutex); 
      s[players[tdL.idThread].session].threads_num -- ;
      if(s[players[tdL.idThread].session].threads_num == 0)
      {
        pthread_cond_broadcast(&cv);
      }
      else
      {
        while(s[players[tdL.idThread].session].threads_num != 0)
        {
          pthread_cond_wait(&cv, &mutex);
        }
      }
      pthread_mutex_unlock(&mutex);
      make_clasament((struct thData*)arg);
      send_clasament((struct thData*)arg);
      send_winner((struct thData*)arg);
    }
		close ((intptr_t)arg);
		return(NULL);
    
};

void quizz(void *arg)
{
  struct thData tdL;
  tdL = *((struct thData *)arg);
  select_question();
  if(write(tdL.cl, &questions_nb, sizeof(int)) == -1)
  {
    printf("[Thread %d]\n", tdL.idThread);
    perror("Eroare la scrierea numarului de intrebari catre client\n");
  }
  for(int i = 0; i < questions_nb; i++)
  {
    sleep(1);
    int aux = id[i];
    if(write(tdL.cl, &aux, sizeof(int)) == -1)
    {
      printf("[Thread %d]\n", tdL.idThread);
      perror("Eroare la scrierea numarului intrebarii catre client\n");
    }
    char aux1[100];
    strcpy(aux1, intrebare[i]);
    if(write(tdL.cl, aux1, sizeof(aux1)) == -1)
    {
      printf("[Thread %d]\n", tdL.idThread);
      perror("Eroare la scrierea intrebarii catre client\n");
    }
    else
    { 
      select_answers(id[i]);
      if(write(tdL.cl, &answers_nb, sizeof(answers_nb)) == -1)
      {
        printf("[Thread %d]\n", tdL.idThread);
        perror("Eroare la scrierea numarului raspunsurilor catre client\n");
      }
      for(int j = 0; j < answers_nb; j++)
      {
        if(write(tdL.cl, raspuns[j], sizeof(raspuns[j])) == -1)
        {
          printf("[Thread %d]\n", tdL.idThread);
          perror("Eroare la scrierea raspunsurilor catre client\n");
        }
      } 
    }
      char rsp[100];
      bzero(&rsp, sizeof(rsp));
      time_t start_time = time(NULL);
      if(read(tdL.cl, rsp, sizeof(rsp)) == -1)
      {
        printf("[Thread %d]\n", tdL.idThread);
        perror("Eroare la citirea raspunsului de la client\n");
      }
      time_t end_time = time(NULL);
      double elapsed_time = difftime(end_time, start_time);
      if (strstr(rsp, "quit") != NULL)
        {
          printf("Clientul [%d] s-a deconectat\n", tdL.idThread);
          players[tdL.idThread].points = -1;
          players[tdL.idThread].giveUp = 1;
          players[tdL.idThread].rank = -1;
          players[tdL.idThread].done = 1;
          s[players[tdL.idThread].session].threads_num--;
          return;
        }
      else  if(elapsed_time > time_limit)
      {
        if(write(tdL.cl, "Timpul a fost depasit, raspunsul nu este luat in considerare", sizeof("Timpul a fost depasit, raspunsul nu este luat in considerare")) == -1)
        {
          printf("[Thread %d]\n", tdL.idThread);
          perror("Eroare la scrierea catre client\n");
        }
      }
     
      else
      {  
        sleep(time_limit - elapsed_time);
        int rasp = atoi(rsp);
        if (rasp == select_correct_answer(id[i]))
        {
          players[tdL.idThread].points = players[tdL.idThread].points + punctaj[i];
          if (write(tdL.cl, "Raspunsul este corect!", sizeof("Raspunsul este corect!")) == -1)
          {
            printf("[Thread %d]\n", tdL.idThread);
            perror("Eroare la scrierea catre client");
          }
        }
      else if (write(tdL.cl, "Raspunsul este gresit", sizeof("Raspunsul este gresit")) == -1)
        {
          printf("[Thread %d]\n", tdL.idThread);
          perror("Eroare la scrierea catre client\n");
        } 
      }
  }
  sleep(1);
  if(write(tdL.cl, "Ai ajuns la finalul intrebarilor!", sizeof("Ai ajuns la finalul intrebarilor!")) == -1)
      {
        perror("Eroare la scrierea mesajului catre client\n");
      }
}
void make_clasament(void *arg)
{
  struct thData tdL;
  tdL = *((struct thData *)arg);
  clasament(tdL.idThread);
  sleep(2);
}
void send_clasament(void *arg)
  {
    struct thData tdL;
    tdL = *((struct thData *)arg);
    int score = players[tdL.idThread].points;
    printf("Scors %d\n", score);
    int rank = players[tdL.idThread].rank;
    printf("Rank %d\n", rank);
    //scorul
    if (write(tdL.cl, &score, sizeof(score)) == -1)
    {
      printf("[Thread %d]\n", tdL.idThread);
      perror("Eroare la scrierea punctajului catre client\n");
    }
    //locul pe care s-a clasa clientul
    if (write(tdL.cl, &rank, sizeof(rank)) == -1)
    {
      printf("[Thread %d]\n", tdL.idThread);
      perror("Eroare la scrierea clasamentului catre client\n");
    }
    
  }
void send_winner(void *arg)
{
  struct thData tdL;
  tdL = *((struct thData *)arg);
  char castigator[100];
  winner(tdL.idThread, castigator);
  //numele castigatorului
  if (write(tdL.cl, castigator, sizeof(castigator)) == -1)
  {
    printf("[Thread %d]\n", tdL.idThread);
    perror("Eroare la scrierea castigatorului catre client\n");
  }
}
void init(void *arg)
{
  struct thData tdL;
  tdL = *((struct thData *)arg);
  char msg[100];
  strcpy(msg, "S-a realizat conectarea la server! Inainte de a incepe, alege un nickname: ");
  if (write(tdL.cl, msg, sizeof(msg)) == -1)
  {
    printf("[Thread %d]\n", tdL.idThread);
    perror("Eroare la scrierea cererii de nickname catre client\n");
  }
  char name[100];
  if(read(tdL.cl, name, sizeof(name)) == -1)
  {
    printf("[Thread %d]\n", tdL.idThread);
    perror("Eroare la citirea numelui introdus de client\n");
  }
  else
  {
    printf("Nickname pentru clientul cu nr %d este: %s\n",tdL.idThread, name);
  }
  initializare(tdL.idThread);
  strcpy(players[tdL.idThread].nickname, name);
}
