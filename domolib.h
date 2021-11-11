
#define MAX_CLIENTS 8
#define SIZE 1024 // dimensione del buffer
#define UDP_PORT 8000
#define TCP_PORT 10000
#define THREAD_LIFETIME	10
#define IP_SIZE 20
#define QUEUELEN 10
#define FILENAME "ip_list.txt"
#define SEMAPH 1
#define DEBUG 0


void TCPserver (void);
void serve_client (int csk, char clnt_ip[]);
void add_device (FILE *fp, char file_name[], char ip[]);
int search_static_ip (FILE *fp, char file_name[], char clnt_ip[]);
int load_devices (FILE *fp, char file_name[], int max_devices);
void print_devices (int n_devices); 
void multithread_f (void);
void *udp_checkstate (void*);
int udp_ping (struct sockaddr_in Srv, struct sockaddr_in fromAddr, int ping_sk, int i);
void handle_error (char *message);

