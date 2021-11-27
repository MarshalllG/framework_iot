
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#define BUFF_SIZE 32
#define MAX_CLIENTS 8
#define IP_SIZE 20
#define IP "192.168.1.102"
#define MSG_TO_SEND "40.54 gradi C"
#define PORT 8000

void handle_request (struct sockaddr_in Clnt, int sk, char buff[], char msg_to_send[]);

int main (int argc, char *argv[])
{
    int sk;
    int received_msg_size;
    char buff[BUFF_SIZE];
    struct sockaddr_in Srv, Clnt;
    unsigned int ClntAddrLen;    
    
    // SOCKET
    if((sk = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        perror("error in socket\n");
        exit (EXIT_FAILURE);
    }

    // UDP SERVER'S INFO
    memset (&Srv, 0, sizeof (Srv));
    Srv.sin_family = AF_INET;
    Srv.sin_addr.s_addr = inet_addr (IP);
    Srv.sin_port = htons (PORT);

    // BIND
    if (bind (sk, (struct sockaddr *) &Srv, sizeof (Srv)) < 0)
    {
        perror ("error in bind\n");
        exit (EXIT_FAILURE);
    }
    printf ("bind successfull to IP: %s, port: %d\n", IP, PORT);    
    
    // finché c'è un input
    do
    {
       printf ("waiting for a message...\n");
       ClntAddrLen = sizeof (Clnt);

       // prepara il buffer
       memset (buff, 0, BUFF_SIZE);
       
       received_msg_size = recvfrom(sk, buff, BUFF_SIZE, 0, (struct sockaddr*) &Clnt, &ClntAddrLen);


       // se recvfrm fallisce
       if (received_msg_size < 0)
       {
          perror ("error in recvfrom\n");
          exit (EXIT_FAILURE);
       }
       else // qualcosa è stato ricevuto
       {
           // debug output
#ifdef DEBUG
          printf("Received message: %s\n from port %d, ip address %s\n", buff, ntohs(Clnt.sin_port), inet_ntoa(Clnt.sin_addr));
#endif    

          handle_request (Clnt, sk, buff, MSG_TO_SEND);
       }

    } while (received_msg_size >= 0);
       
    // close socket
    close(sk);
    
    return EXIT_SUCCESS;
}


void handle_request (struct sockaddr_in Clnt, int sk, char buff[], char msg_to_send[])
{
    if (strlen (buff) > 0)
    {
        printf ("strlen(buff): %lu\n", strlen(buff));
             
        printf ("sending reply...\n");
        if (sendto(sk, msg_to_send, strlen(msg_to_send), 0, (struct sockaddr *) &Clnt, sizeof (Clnt)) <= 0)
        {
            perror ("error in send\n");
            exit (EXIT_FAILURE);
        }
           
        printf("msg_sent: %s\n", msg_to_send);
    }
    return;
}
