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
#include <math.h>

#define SO_ORIGINAL_DST 80

#define MAX(a,b) ((a)>(b)?(a):(b))

pthread_mutex_t lock;

/* 
 * Processes the client request through the proxy to the server and back again
 * Reads the request 
 * Extracts ip/port from client, extracts ip/port from desitnation 
 * Creates socket to connect proxy to server
 * Binds this socket to get a port number for the proxy side of the connection
 * Adds SNAT entry from proxy to server
 * Connect socket between proxy and server
 * Relays information between client and server using select statement until session is ended
 * Deletes SNAT entry and logs information in the proxy.log file
 * Closes both socket connections
 */

void *processRequest(void *s) { //,char *document_root) {
    
    /* Help from here
     * http://ubuntuforums.org/showthread.php?t=634216
     */
    
    
    int client_sock = *((int *) s);
    free(s);
    
    // Get the information about address and port numbers from sockets
    struct sockaddr_in dest_addr;
    socklen_t dest_len = sizeof(dest_addr);
    memset(&dest_addr, 0, dest_len);
    
    getsockopt(client_sock, SOL_IP, SO_ORIGINAL_DST, &dest_addr, &dest_len);
    
    struct sockaddr_in proxy_addr;
    socklen_t proxy_len = sizeof(proxy_addr);
    memset(&proxy_addr, 0, proxy_len);
    
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    memset(&client_addr, 0, client_len);
    
    getpeername(client_sock,(struct sockaddr *)&client_addr,&client_len);
    

    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(0);
    inet_aton("10.0.0.1", &server_addr.sin_addr);
    
    int server_sock = socket(AF_INET,SOCK_STREAM,0);
    
    //bind to get a port number for the proxy-server connection on the proxy side
    if (bind(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(-1);
    }
    
    getsockname(server_sock,(struct sockaddr *)&proxy_addr,&proxy_len);
    
    //add iptable rule
    pthread_mutex_lock(&lock);
    char comm[1000];
    snprintf(comm, sizeof(comm), "iptables -t nat -A POSTROUTING -p tcp -j SNAT --sport %hu --to-source %s", ntohs(proxy_addr.sin_port),inet_ntoa(client_addr.sin_addr));
    system(comm);
    pthread_mutex_unlock(&lock);
    
    //printf( "Server Destination: %s:%hu\n", inet_ntoa(proxy_addr.sin_addr), ntohs(proxy_addr.sin_port));
    
    //printf( "Client Source: %s:%hu\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    
    //printf( "Client Destination: %s:%hu\n", inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port));
    
    short orig_port = ntohs(dest_addr.sin_port);
    char orig_addr[64];
    sprintf(orig_addr,"%s",inet_ntoa(dest_addr.sin_addr));
    
    struct tm *ptr;
    time_t lt;
    char str[100];
    lt = time(NULL);
    ptr = localtime(&lt);

    strftime(str, 100, "%D %H:%M:%S", ptr); //get date and time
   
    if(connect(server_sock,(struct sockaddr *)&dest_addr,dest_len)<0){
        perror("connect");
        exit(-1);
    }
    
    long total_bytes_sent=0;
    long total_bytes_received=0;

    
    //set up socket set for communication
    int sel_ret;
    
    struct timeval tv;
    fd_set socket_set;
    
    FD_ZERO(&socket_set);
    char buf[1024];
    memset(buf, '\0', 1024);
    long bytes_client=0;
    long bytes_server=0;
    
    
    while(1){//communicate link
        FD_ZERO(&socket_set);
        FD_SET(client_sock,&socket_set);
        FD_SET(server_sock,&socket_set);
        tv.tv_sec = 10;
        tv.tv_usec = 0;

        sel_ret = select(MAX(client_sock,server_sock)+1,&socket_set,NULL,NULL,&tv);

        if(sel_ret>=1){
            if(FD_ISSET(client_sock,&socket_set)){
                bytes_client = recv(client_sock,buf,1024,0);
                send(server_sock,buf,bytes_client,0);
                total_bytes_sent+=bytes_client;
		if(bytes_client<=0){ //session over
            //printf("read 0 client\n");
			break;
		}
			
            }
            if(FD_ISSET(server_sock,&socket_set)){
                bytes_server = recv(server_sock,buf,1024,0);
                send(client_sock,buf,bytes_server,0);
                total_bytes_received+=bytes_server;
		if(bytes_server<=0){ //session over
			//printf("read 0 client\n");
			break;
		}
            }
        }
        

        
    }
    
     //clean up
    close(client_sock);
    close(server_sock);
    //printf("at cleanup\n");
    pthread_mutex_lock(&lock);
    snprintf(comm, sizeof(comm), "iptables -t nat -D POSTROUTING -p tcp -j SNAT --sport %hu --to-source %s", ntohs(proxy_addr.sin_port),inet_ntoa(client_addr.sin_addr));
    system(comm);
    
    //append onto log file
    FILE *fp = fopen("proxy.log","a");

    char log[2048];
    sprintf(log,"%s %s %hu %s %hu %lu %lu\n",str,inet_ntoa(client_addr.sin_addr),
            ntohs(client_addr.sin_port),orig_addr,orig_port,
            total_bytes_sent,total_bytes_received);
    fwrite(log,sizeof(char),strlen(log),fp);
    fclose(fp);
    pthread_mutex_unlock(&lock);
    
    
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
        perror("bind");
        exit(-1);
    }
    
    if((listen(sock,5))==-1)
    {
        perror("listen");
        exit(-1);
    }
    //int i = 0;
    while(1)
    {
        if((cli = accept(sock, (struct sockaddr *)&client,&len))==-1)
        {
            perror("accept");
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
        //Clear nat tables and then add DNAT
        int port = atoi(argv[1]);
        char clear = "iptables -t nat -F";
        char comm[1000];
        snprintf(comm, sizeof(comm), "iptables -t nat -A PREROUTING -p tcp -i eth0 -j DNAT --to 192.168.0.1:%d", port);
        system(clear); //clear any prerouting rules, we only want ours
        system(comm);
        //FILE *fp = fopen("proxy.log","w"); //create a new one for each time running server
        //fclose(fp);
        run_server(port);
    }
    return 0;
}
