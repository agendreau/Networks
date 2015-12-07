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

/* Reads the response header from the server using the readline function
 * Stores the response header in the response string
 * Writes the response to a file for caching
 * Returns...
 *  The content length of the response body
 *  -1 if there is an error in reading the response
 *  -2 if there is no Content-Length field in the response (note content length is required by HTTP/1.0
 */
long readRequestProxy(int server_sock, char response[]) {
    char buf[1024];
    char length[65];
    memset(length, '\0', 65);
    memset(buf, '\0', 1024);
    long content_length=-2;
    int open;
    
    
    while(1) {
        open = readLine(buf,server_sock);
        if(open<=0){ //error reading from serving
            //printf("here in response\n");
            content_length=-1;
            return content_length;
        }
        else if (strcmp(buf,"\r\n")==0) { //reached end of response header
            strcat(response,buf);
            return content_length;
        }
        else if(strncmp(buf,"Content-Length:",15)==0){
            
            strcat(response,buf);
            int i=16;
            while(buf[i]!='\r'){
                length[i-16]=buf[i];
                i++;
            }
            printf("content length:%s\n",length);
            content_length=atol(length);
            printf("content length:%lu\n",content_length);
        }
        
        
        else {
            //printf("the buffer is: %s\n",buf);
            strcat(response,buf);
        }
    }
    
}



/* Reads the request from the client
 * Stores entire request in request string
 * Return the content length of the request body if one exists
 * If one exists it must be specified in the header field: Content-Length
 */

long readRequest(int sock,char request[]) {
    
    char buf[1024];
    char length[65];
    memset(length, '\0', 65);
    memset(buf, '\0', 1024);
    long content_length=0;
    int open;
    
    
    while(1) {
        open = readLine(buf,sock);
        if(open<=0){
            content_length=-1;
            return content_length;
        }
        else if(strncmp(buf,"Content-Length:",15)==0){
            
            strcat(request,buf);
            
            int i=16;
            while(buf[i]!='\r'){
                length[i-16]=buf[i];
                i++;
            }
            printf("content length:%s\n",length);
            content_length=atol(length);
            printf("content length:%lu\n",content_length);
        }
        
        else if(strcmp(buf,"\r\n")==0){
            strcat(request,buf);
            return content_length;
        }
        else {
            strcat(request,buf);
        }
        
    }
    
}


/* Processes the client request
 * Reads the request
 * Checks for errors in request and sends back the correct error message
 * Closes the connection immediately after message is passed through the server and back to the client
 * Parses the request, host, uri, and port
 */

void *processRequest(void *s) { //,char *document_root) {
    
    /* Help from here
     * http://ubuntuforums.org/showthread.php?t=634216
     */
    
    //Setting everything up.  Not the best way to manage my memory
    int client_sock = *((int *) s);
    free(s);
    
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
    
    printf( "Server Destination: %s:%hu\n", inet_ntoa(proxy_addr.sin_addr), ntohs(proxy_addr.sin_port));
    
    printf( "Client Source: %s:%hu\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    
    printf( "Client Destination: %s:%hu\n", inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port));
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(0);
    inet_aton("10.0.0.1", &server_addr.sin_addr);
    
    int server_sock = socket(AF_INET,SOCK_STREAM,0);
    
    if (bind(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }
    
    getsockname(server_sock,(struct sockaddr *)&proxy_addr,&proxy_len);
    
    char comm[1000];
    snprintf(comm, sizeof(comm), "“iptables -t nat -A POSTROUTING -p tcp -j SNAT --sport %hu --to-source %s", ntohs(proxy_addr.sin_port),inet_ntoa(client_addr.sin_addr));
    system(comm);
    
    
    
    
    // Now connect to the server
    if (connect(server_sock, (struct sockaddr *)&dest_addr, dest_len) < 0) {
        printf("ERROR connecting to server \n");
        //exit(1);
    }
    
    int select_client;
    int select_server;
    
    struct timeval tv;
    fd_set set_client;
    fd_set set_server;
    
    FD_ZERO(&set_client);
    FD_ZERO(&set_server);
    char buf[1024];
    memset(buf, '\0', 1024);

    
    while(1){
        FD_ZERO(&set_client);
        FD_SET(client_sock,&set_client);
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        
        select_client = select(client_sock+1,&set_client,NULL,NULL,&tv);
        //printf("fdset: %d\n",selRet);
        //selRet=2; //for testing error 500
        if(select_client==0) {//timeout :(
            printf("we timed out.\n");
            break;
        }
        
        else if(select_client==1){
            long bytes_read = recv(client_sock,buf,1024,0);
            send(server_sock,buf,bytes_read,0);
        }
        
    }
    
    while(1){
        FD_ZERO(&set_server);
        FD_SET(server_sock,&set_server);
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        
        select_server = select(server_sock+1,&set_server,NULL,NULL,&tv);
        //printf("fdset: %d\n",selRet);
        //selRet=2; //for testing error 500
        if(select_server==0) {//timeout :(
            printf("we timed out.\n");
            break;
        }
        
        else if(select_server==1){
            long bytes_read = recv(server_sock,buf,1024,0);
            send(client_sock,buf,bytes_read,0);
        }

        
    }
    
    //Communicate between the server and the client
    
    /*char request[4096];
    memset(request, '\0', 4096);
    int content_length = readRequest(client_sock,request);
    printf("Request\n%s\n",request);
    int sent = send(server_sock,request,strlen(request),0);
    printf("success: %d\n",sent);
    
    char response[4096];
    memset(response, '\0', sizeof(response));
    long response_content = readRequestProxy(server_sock,response);
    printf("Response\n%s\n",response);
    printf("Response content: %lu\n",response_content);
    send(client_sock,response,strlen(response),0);
    
    char buf[1025];
    size_t total_read_bytes=0;
    size_t read_bytes=0;
    while(total_read_bytes<response_content){
        read_bytes = recv(server_sock, buf,1024,0);//send file
        total_read_bytes+=read_bytes;
        send(client_sock,buf,read_bytes,0);
    }*/


    
    
    close(server_sock);
    
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
        snprintf(comm, sizeof(comm), "iptables -t nat -A PREROUTING -p tcp -i eth0 -j DNAT --to 192.168.0.1:%s", argv[1]);
        system(comm);
        run_server(atoi(argv[1]));
    }
    return 0;
}
