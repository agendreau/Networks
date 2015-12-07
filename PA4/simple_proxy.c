#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/stat.h>
#include <netinet/ip.h>

#define SO_ORIGINAL_DST 80




/* read line from socket character by character */
int readLine(char firstLine[], int sock) {
    int i = 0;
    char c;
    int len;
    len = recv(sock, &c, 1, 0);
    //printf("len: %d\n",len);
    while (len == 1 && c != '\n') {
        firstLine[i] = c;;
        i++;
        len = recv(sock, &c, 1, 0);
    }
    firstLine[i] = '\n';
    firstLine[i+1] = '\0';
    //printf("readLine: %s",firstLine);
    return len;
    
}


/* Processes the client request
 * Reads the request
 * Checks for errors in request and sends back the correct error message
 * Closes the connection immediately after message is passed through the server and back to the client
 * Parses the request, host, uri, and port
 */

void *processRequest(void *s) { //,char *document_root) {
    
    //Setting everything up.  Not the best way to manage my memory
    int client_sock = *((int *) s);
    free(s);
    
    struct sockaddr_in dest_addr;
    socklen_t dest_len = sizeof(dest_addr);
    memset(&dest_addr, 0, dest_len);
    
    getsockopt(client_sock, SOL_IP, SO_ORIGINAL_DST, &dest_addr, &dest_len);
    
    printf( "Client: %s:%hu\n", inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port));
    
    /*
    int proxyClient;
    struct addrinfo hints, *res;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    //char * port = "80";
    //host[strlen(host)-2]='\0'; //get rid of the \r\n
    //printf("Actual host: %s\n",host);
    
    getaddrinfo(host, port, &hints, &res);
    getpeerName(client_sock) = "host";
    //get source port of client oscket
    
    //seg fault if server isn't running (i.e. get address info of inactive port)
    proxyClient = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    
    //get port it will try to connect on?
    
    proxyClient.bind(("10.0.0.1", 0));
    
    
    
    
    // Now connect to the server
    if (connect(proxyClient, res->ai_addr, res->ai_addrlen) < 0) {
        printf("ERROR connecting to server %s\n",port);
        //exit(1);
    }
    
    //Communicate between the server and the client
    communicate(client_sock,proxyClient,request,uri,content_length);
    close(proxyClient); */
    
    
   
    
    close(client_sock);
    
    return NULL;
    
}



void run_server(int port)
{
    /* some of this code was written after watching the video lectures from
     * https://www.youtube.com/watch?v=eVYsIolL2gE
     */
    int sock, cli;
    struct sockaddr_in server,client;
    unsigned int len;
    //char mesg[] = "Hello to the world of socket programming";
    int *sent;
    pthread_t t;
    
    if((sock = socket(AF_INET,SOCK_STREAM,0)) == -1)
    {
        printf("socket: \n");
        exit(-1);
    }
    
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&server.sin_zero,8);
    
    len = sizeof(struct sockaddr_in);
    
    if((bind(sock,(struct sockaddr *)&server,len)) == -1)
    {
        printf("bind\n");
        exit(-1);
    }
    
    if((listen(sock,5))==-1)
    {
        printf("listen\n");
        exit(-1);
    }
    //int i = 0;
    while(1)
    {
        if((cli = accept(sock, (struct sockaddr *)&client,&len))==-1)
        {
            printf("accept\n");
            exit(-1);
        }
        //i++;
        //printf("total requests: %d\n",i);
        sent = (int *) malloc(sizeof(int));
        *sent = cli;
        pthread_create(&t,NULL,processRequest,(void *) sent);
        //processRequest((void *) sent);
        //        free(sent);
        
    }
    close(sock);
    
}

int main(int argc, char *argv[]){
    if(argc==1){
        printf("You need to specifiy a port for the proxy\n");
        exit(0);
    }
    else {
        //Add DNAT rule
        char comm[1000];
        snprintf(comm, sizeof(comm), "iptables -t nat -A PREROUTING -p tcp -i eth0 -j DNAT --to 192168.0.1:%s", argv[1]);
        system(comm);
        run_server(atoi(argv[1]));
    }
    return 0;
}
