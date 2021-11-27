#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h> // include i semafori, si usano per sincronizzare le operazioni dei thread
#include "masterlib.h"
#define IP "192.168.1.100"
#define UDP_MSG_TO_SEND "Qual è la temperatura rilevata dal sensore?"



/* struct che raccoglie i vari dispositivi nella rete, i campi sono l'ip e lo stato */
struct device
{
   char ipaddr[IP_SIZE];
   int connected;
   char dev_state[SIZE]; // informazioni di stato
};

/* argomento da passare a pthread_create: struttura che contiene gli indici da usare nel thread */
struct thread_info 
{
   pthread_t thread_id;        /* ID del thread restituito da pthread_create() */
   int thread_num;       /* indice */
   int tot_ip;     /* numero degli ip registrati */
};

// dichiarazione di variabili globali
struct device d[MAX_CLIENTS];
#ifdef SEMAPH
sem_t semaph[MAX_CLIENTS]; // dichiarazione di un vettore di semafori, la dimensione massima è il numero massimo di client accettati
#endif


/*********************************************************************************
 *********************************************************************************
 FUNZIONI PER LE COMUNICAZIONI IN TCP
 *********************************************************************************
 ********************************************************************************/


void TCPserver (void)
{
   struct sockaddr_in saddr, caddr; 
   int ready = 1; //flag
   int rc; // per il thread
   int listensk, sk; // uso due socket: uno di ascolto e uno per la connessione
   socklen_t addr_size; // per l'accept
   pid_t pid; // process identifier della funzione fork
   pthread_t thread; // singolo thread per comunicazioni UDP
   char clnt_ip[IP_SIZE];



   // SOCKET
   if ((listensk = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      handle_error("error in socket\n");
   printf("TCP server socket created\n");

   // SERVER INFO
   memset(&saddr, '\0', sizeof(saddr));
   saddr.sin_family = AF_INET;
   saddr.sin_port = TCP_PORT;
   saddr.sin_addr.s_addr = inet_addr(IP);

   // BIND
   if ((bind(listensk, (struct sockaddr*)&saddr, sizeof(saddr))) < 0)
      handle_error("error in bind\n");
   printf("bind to: ip %s port %d\n", IP, TCP_PORT);

   // LISTEN
   if (listen (listensk, QUEUELEN) < 0)
      handle_error("error in listen\n");
   printf("listening...\n");


   // LOOP
   do
   {    
      /* prepara il server a connessioni */ 
      ready = 1;
          
      // ACCEPT CLIENT
      addr_size = sizeof(caddr);
      if ((sk = accept(listensk, (struct sockaddr*)&caddr, &addr_size)) < 0)
         handle_error("error in accept\n");
      printf("connection accepted, client has ip %s port %d\n", inet_ntoa(caddr.sin_addr), caddr.sin_port);
        
 
      // FORK
      if ((pid = fork()) == -1)
      {
         printf ("\nThis is parent: error creating subprocess\n");
         ready = 0;
      }

      if (ready == 1)
      {
      	// CHILD WILL SERVE THE CLIENT (and then, it will terminate)
      	if (pid == 0)
      	{
           close (listensk);
           strcpy (clnt_ip, inet_ntoa(caddr.sin_addr));
           serve_client (sk, clnt_ip);
           close (sk);
           printf ("client disconnected\n");
           exit(0); // se non chiudo il processo mi da' errore sull'accept
        }
      }

      close (sk);

        
   }while (1);

return;
}
    
 
/* ip_list.txt e' un file in cui sono contenuti tutti gli ip che si sono connessi al server in precedenza */

void serve_client (int csk, char clnt_ip[])
{
   char buffer[SIZE];
   FILE *fp;
   int n_ip;
   int i = 0;


/*** DEBUG
   printf ("in serve_client: clnt_ip = %s\n", clnt_ip);
***/

   /*< se non presente nel file, aggiunge il client attualmente connesso alla lista */
   if (search_static_ip (fp, FILENAME, clnt_ip) == 0)
      add_device (fp, FILENAME, clnt_ip);
 
   //restituisce il numero degli ip registrati
   n_ip = load_devices (fp, FILENAME, MAX_CLIENTS); 

/*** DEBUG
   print_devices (n_ip);
   printf ("numero di ip registrati: %d\n", n_ip);
***/

   // dialoga in TCP con il client
   bzero(buffer, SIZE);
   strcpy(buffer, "client connesso: di quale dispositivo devo verificare la temperatura?\nOPTIONS:\n-1\n-2\n");
   send(csk, buffer, strlen(buffer), 0);

   bzero(buffer, SIZE);
   recv(csk, buffer, sizeof(buffer), 0);
   if (atoi(buffer) != 0)
      i = atoi(buffer);

   if (i > sizeof(d))
   {
      bzero(buffer, SIZE);
      strcpy(buffer, "errore, dispositivo selezionato non esistente\n");
      send(csk, buffer, strlen(buffer), 0);
      perror ("errore, dispositivo selezionato non esistente\n");
   }

   printf("il client vuole conoscere la temperatura del device n°%s\n", buffer);

   // chiamata della funzione per verificare con UDP lo stato di ogni dispositivo
   multithread_f();

   if (atoi(buffer) != 0)
   {
      bzero(buffer, SIZE);
      strcpy(buffer, d[i].dev_state);
      send(csk, buffer, strlen(buffer), 0);
      printf ("messaggio spedito al client TCP: %s\n", buffer);
   }
   else
   {
      bzero (buffer, SIZE);
      printf ("Option not avaiable, closing connection...\n");
      strcpy (buffer, "Option not avaiable, closing connection...\n");
      send (csk, buffer, strlen(buffer), 0);
   }
    
   return;
}



/*********************************************************************************
 *********************************************************************************
 FUNZIONI PER LE COMUNICAZIONI IN UDP
 *********************************************************************************
 ********************************************************************************/
void multithread_f (void)
{
   FILE *fp;
   pthread_t threads[MAX_CLIENTS];
   int i, n_ip, rc, tnum, res;
 
   // restituisce il numero degli ip registrati
   n_ip = load_devices (fp, FILENAME, MAX_CLIENTS); 

/*** DEBUG
   printf("print_devices inizia ora:\n");
   print_devices (n_ip);
   printf ("numero di ip registrati: %d\n", n_ip);
***/

   // allocazione di memoria per gli argomenti della pthread_create
   struct thread_info *tinfo = calloc(MAX_CLIENTS, sizeof(*tinfo));
   if (tinfo == NULL)
      handle_error("calloc");

   // inizializzo un semaforo sbloccato
   if ((res = sem_init (&semaph[1], 0, 1)) == -1)   
   {   
        printf("Cannot create semaphore. Returned code is: %d\n", res);
        printf("%s\n", strerror(errno));
        return;
   }

   for (i = 2; i <= n_ip; i++)
   {
      // inizializzo gli altri semafori bloccati
      if ((res = sem_init (&semaph[tnum], 0, 0)) == -1)      
         handle_error("Cannot create semaphore.\n");
   }

   // CICLO FOR PER LA CREAZIONE DEI THREAD
   for (tnum = 1; tnum <= n_ip; tnum++)
   {
      // scrivo gli indici nella struttura dati di info, argomento per i thread
      tinfo[tnum].thread_num = tnum;    

      // 1° argomento: scrivo l'id del thread in un campo della struct
      // 3° argomento: funzione che contiene il codice in base a cui avviene l'elaborazione
      // 4° argomento: passa una struttura dati come argomento per la checkstate
      rc = pthread_create (&tinfo[tnum].thread_id, NULL, udp_checkstate, &tinfo[tnum]);

      printf("created thread %d, id %ld\n", tnum, (long int) tinfo[tnum].thread_id);

      if (rc)
      {
         printf("ERROR: return code from pthread_create() is %d\n", rc);
         exit (EXIT_FAILURE);
      }
   }
   //////////////////////////////////////////////////////////////////////////////////


#ifdef SEMAPH
   for (i = 1; i <= MAX_CLIENTS; i++)
      sem_destroy(&semaph[i]);
#endif


   sleep(1 + 2 * THREAD_LIFETIME);
/*** DEBUG
   printf ("esco dalla udp_multithread\n");
   return;
***/
}



// riceve l'indice di un thread e avvia una comunicazione UDP per verificare lo stato
void *udp_checkstate (void *arg)
{
   struct thread_info *tinfo = arg;

   // per comunicare in UDP
   struct sockaddr_in Srv, fromAddr;
   int target_state;
   int ping_res;
   int sk;
   int index = tinfo->thread_num; // prendo l'indice dalla struct passata come argomento
   int n_ip = tinfo->tot_ip; // prelevo il valore del totale di ip registrati


/*** DEBUG
   printf ("siamo nella udp_checkstate\n");
   printf("Thread %d: ", tinfo->thread_num);
   printf ("%d\n", index);
***/

   // SOCKET
   if((sk = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
      handle_error("error in socket\n");

/*** DEBUG
   printf ("d[%d].ipaddr: %s\n", index, d[index].ipaddr);
***/

   // info sul server
   memset(&Srv, 0, sizeof(Srv));
   Srv.sin_family = PF_INET;
   Srv.sin_port = htons (UDP_PORT);
   Srv.sin_addr.s_addr = inet_addr(d[index].ipaddr);


#ifdef SEMAPH
   // funzione bloccante per gli altri thread
   // con la wait: se il valore del semaforo è 0 (terzo parametro di inizializzazione), la funzione è bloccante per quella risorsa
   sem_wait(&semaph[index]);
#endif


   // assegna gli stati
   if ((ping_res = send_udp_message (Srv, fromAddr, sk, index)) == 1)
   {
/* DEBUG   
      printf ("udp_ping returned: %d\n", ping_res);
*/
      target_state = 1;
      printf ("valore misurato presso il dispositivo n°%d: %s\n", index, d[index].dev_state);
   }
   else
   {
/* DEBUG   
      printf ("udp_ping returned: %d\n", ping_res);
*/
      target_state = 0;
   }

   if (target_state == 1)
      printf ("state of target is: active\n");
   else if (target_state == 0)
      printf ("state of target is: inactive\n");


   
   
#ifdef SEMAPH
   // post è sbloccante e va a sbloccare la risorsa nel prossimo thread
     sem_post(&semaph[index+1]);
#endif


   close (sk);

/*** DEBUG
   printf ("esco dal thread\n");
***/

   pthread_exit (NULL);
}






// riceve le struct e il socket, restituisce 1 se il dispositivo è attivo, 0 altrimenti
int send_udp_message (struct sockaddr_in Srv, struct sockaddr_in fromAddr, int ping_sk, int i)
{
   int msg_len = strlen (UDP_MSG_TO_SEND);
   int timeout = 10;
   char udp_response[SIZE]; 
   unsigned int fromSize;
   struct timeval read_timeout;

   read_timeout.tv_sec = timeout;
   read_timeout.tv_usec = 0;

   setsockopt(ping_sk, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof (struct timeval));

/*** DEBUG
   printf ("UDP_MSG_TO_SEND: %s\n", UDP_MSG_TO_SEND);
***/

   // invia un messaggio udp
   if (sendto(ping_sk, UDP_MSG_TO_SEND, sizeof(UDP_MSG_TO_SEND), 0, (struct sockaddr *) &Srv, sizeof(Srv)) < 0)
   {
      printf("udp message error, can't send buff\n");
      exit (EXIT_FAILURE);
   }
   printf ("msg sent to device n°%d: %s\n", i, UDP_MSG_TO_SEND);

   // se ricevo risposta restituisco 1
   fromSize = sizeof (fromAddr);
   recvfrom(ping_sk, udp_response, SIZE, 0, (struct sockaddr*) &fromAddr, &fromSize);

   if (strlen(udp_response) > 0)
   {
      strcpy(d[i].dev_state, udp_response);
      return 1;
   }
   else if (strlen(udp_response) == 0)
   {
      printf ("nothing was received\n");
      return 0;
   }

   return 0;
}


///////////////////////////////////////////////
//FUNZIONI DI UTILITY//////////////////////////
///////////////////////////////////////////////

void add_device (FILE *fp, char file_name[], char ip[])
{
   // apertura del file in modalità "append"
   if ((fp = fopen(FILENAME, "a")) == NULL)
   {
      fprintf (stderr, "not able to open file %s\n", file_name);
      exit (EXIT_FAILURE);
   }

   // aggiunge l'ip
   fprintf (fp, "%s\n", ip);
   printf ("adding ip %s\n", ip);
   fclose (fp);

   return;
}





// carica un vettore di struct device leggendo da file
int load_devices (FILE *fp, char file_name[], int max_devices)
{
   int i = 1;
   char tmp_string[IP_SIZE][MAX_CLIENTS];
   
   // apertura del file in modalità di lettura
   if ((fp = fopen (file_name, "r")) == NULL)
   {
      printf ("cannot open file %s, error in file opening\n", file_name);
      exit (EXIT_FAILURE);
   }

   while ((i < max_devices) && (fgets (tmp_string[i], IP_SIZE, fp) != NULL))
   {
/*** DEBUG
      printf ("we are in function load_devices, max_devices=%d, i=%d\n", max_devices, i);
      printf ("in the file %s, ip = %s\n", file_name, tmp_string);
***/
      if (strlen(tmp_string[i]) > 2)
         strcpy (d[i].ipaddr, tmp_string[i]);
     
/*** DEBUG
      printf ("d[%d].ipaddr: %s\n", i, d[i].ipaddr);
***/

      d[i].connected = 1;
      i++; 
   }
 
/*** DEBUG
   printf ("end of function load_devices\n");
***/
   // chiude il file
   fclose(fp);
   
   return (i-1);
} 





// stampa il vettore di struct
void print_devices (int n_devices)
{
   int i;
   
   for (i = 1; i <= n_devices; i++)
   {
      printf ("d[%d].ipaddr = %s\n", i, d[i].ipaddr);
      printf ("d[%d].dev_state = %s\n", i, d[i].dev_state);
   }
   
   return;
}






// cerca un ip nel file: se lo trova restituisce 1, 0 altrimenti 
int search_static_ip (FILE *fp, char file_name[], char clnt_ip[])
{
   int i;
   char tmp[SIZE];
    
   // apertura del file in modalità di lettura
   if ((fp = fopen(FILENAME, "r")) == NULL)
   {
      fprintf (stderr, "not able to open file %s\n", file_name);
      exit (EXIT_FAILURE);
   }
   
   while (fscanf (fp, "%s", tmp) != EOF)
   {
/*** DEBUG
      printf ("confronto le stringhe client_ip: %s e tmp: %s\n", clnt_ip, tmp);
***/
      if (strcmp (clnt_ip, tmp) == 0)
      {
/*** DEBUG
         printf ("ho trovato questo ip nel file, quindi restituisco 1\n");
***/
         return 1;
      }
   }

   // chiude il file
   fclose(fp);
/*** DEBUG
   printf ("non ho trovato questo ip nel file, quindi restituisco 0\n");
***/
   return 0;
}

void handle_error (char *message)
{
   printf ("fatal error: %s\n", message);
   exit (EXIT_FAILURE);
}


