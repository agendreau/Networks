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
/* Facilitates communication between the server and the client
 * First checks if the requested file is cached
 *  if it is and the cahce hasn't expired, the file is immmediately sent to the client
 *  otherwise the resquest is sent to the server and the response sent back to the client
 *  storing a copy in the cache
 */
long communicate(int client_sock,int proxy_sock,char * request, long content_length){
    
    char buf[1024];
    ssize_t total_read_bytes=0;
    ssize_t read_bytes;
    ssize_t bytes_sent;
    
    char * status1 = "HTTP/1.1 500 Internal Server Error: Not responsive\r\n\r\n";
    int j = send(proxy_sock,request,strlen(request),0);
    if(content_length>0){//request has a body, send it to the server (even though it is meaningless)
        read_bytes=0;
        while(total_read_bytes<content_length){
            read_bytes = recv(client_sock,buf,1024,0);
            total_read_bytes+=read_bytes;
            bytes_sent = send(proxy_sock,buf,read_bytes,0);
        }
        
    }
    if (j<=0){
        printf("request failed to send\n");
        send(client_sock,status1,strlen(status1),0);
        return 0;
    }
    else {
        printf("request sent: %d\n",j);
        char response[4096];
        memset(response, '\0', sizeof(response));
        long response_content = readRequestProxy(proxy_sock,response);
        printf("Response\n%s\n",response);
        printf("Response content: %lu\n",response_content);
        if(response_content==-1){
            printf("error in receiving request from server\n");
            send(client_sock,status1,strlen(status1),0);
            return 0;
        }
        else if(response_content==-2){
            printf("error in receiving request from server\n");
            char * status = "HTTP/1.0 400 Bad Request: Invalid Content Length: \r\n\r\n";
            send(client_sock,status,strlen(status),0);
            return 0;
        }
        else {
            //printf("we made it here\n");
            bytes_sent=send(client_sock,response,strlen(response),0);
            if(bytes_sent<=0){
                printf("error in sending response to client\n");
                return 0;
            }
            else {
                total_read_bytes=0;
                while(total_read_bytes<response_content){
                    read_bytes = recv(proxy_sock, buf,1024,0);//send file
                    if(read_bytes<=0){
                        printf("error in reading file from server\n");
                        return 0;
                    }
                    total_read_bytes+=read_bytes;
                    bytes_sent = send(client_sock,buf,read_bytes,0);
                    if(bytes_sent<=0){
                        printf("error in sending file to client\n");
                        return 0;
                    }
                }
                return total_read_bytes;
                //printf("out of while\n");
            }
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

void processHTTPRequest(int client_sock,int server_sock,long * total_bytes_received,long * total_bytes_sent) { //,char *document_root) {
    
    
    char request[4096];
    char host[128];
    char port[64];
    char uri[1024];
    char status1[1024];
    
    
    memset(request, '\0', 4096);
    memset(host, '\0', 128);
    memset(uri, '\0', 1024);
    memset(port, '\0', 16);
    memset(status1, '\0', 1024);
    
    
    
    int content_length = readRequest(client_sock,request);
    
    printf("Request\n%s\n",request);
    //check for errors
    if(content_length==-1){
        printf("we want to close the socket on an empty request call\n");
        return;
       
    }
    
    
    /* Am I an GET method? */
    if(strncmp(request,"GET",3)!=0 &&
       strncmp(request,"get",3)!=0 &&
       strncmp(request,"Get",3)!=0){
        printf("Bad Method\n");
        sprintf(status1,"HTTP/1.0 400 Bad Request: Invalid Method: Not Get\r\n\r\n");
        send(client_sock,status1,strlen(status1),0);
        printf("we want to close the socket on bad method request\n");
        
        return;
        //break;
    }
    
    *total_bytes_received = communicate(client_sock,server_sock,request,content_length);
    *total_bytes_sent = strlen(request)+content_length;
    
    
    
   
    
    
    
}

void processSSHRequest(int client_sock,int server_sock,long * total_bytes_received,long * total_bytes_sent){
    
    
    /* read line from socket character by character */
    char request[1024];
    char buf[1024];
    memset(request, '\0', 1024);
    memset(buf, '\0', 1024);
    long tr=0;
    long ts=0;
    while(1){
        if(strcmp(request,"exit\n")==0){
            readLine(request, client_sock);
            send(server_sock,request,strlen(request),0);
            long bytes_server = recv(server_sock,buf,1024,0);
            send(client_sock,buf,strlen(buf),0);
            break;
        }
        
        readLine(request, client_sock);
        send(server_sock,request,strlen(request),0);
        
    
        int select_server;
    
        struct timeval tv;
        fd_set set_server;
    
        
        FD_ZERO(&set_server);

    
        long bytes_client=0;
        long bytes_server=0;
    

    
        while(1){
            FD_ZERO(&set_server);
            FD_SET(server_sock,&set_server);
            tv.tv_sec = 10;
            tv.tv_usec = 0;
        
            select_server = select(server_sock+1,&set_server,NULL,NULL,&tv);

            if(select_server==0) {//timeout :(
                printf("we timed out.\n");
                break;
            }
        
            else if(select_server==1){
                bytes_server = recv(server_sock,buf,1024,0);
                send(client_sock,buf,bytes_server,0);
                tr+=bytes_server;
            }
            
            else if(select_server<0){
                printf("error in selecting socket from set\n");
                break;
            }
        
        }
    }
    *total_bytes_sent=ts;
    *total_bytes_received=tr;

    
}

void processFTPRequest(int client_sock,int server_sock,long * total_bytes_received,long * total_bytes_sent){
    processSSHRequest(client_sock,server_sock,total_bytes_received,total_bytes_sent);
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
    snprintf(comm, sizeof(comm), "iptables -t nat -A POSTROUTING -p tcp -j SNAT --sport %hu --to-source %s", ntohs(proxy_addr.sin_port),inet_ntoa(client_addr.sin_addr));
    system(comm);
    
    printf( "Server Destination: %s:%hu\n", inet_ntoa(proxy_addr.sin_addr), ntohs(proxy_addr.sin_port));
    
    printf( "Client Source: %s:%hu\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    
    printf( "Client Destination: %s:%hu\n", inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port));
    
    
    int port = ntohs(dest_addr.sin_port);
    printf("port number: %d\n",port==8080);
    
    struct tm *ptr;
    time_t lt;
    char str[100];
    lt = time(NULL);
    ptr = localtime(&lt);

    strftime(str, 100, "%D %H:%M:%S", ptr);
    
    long total_bytes_sent=0;
    long total_bytes_received=0;

    
    if(port==22) {//ssh
        if (connect(server_sock, (struct sockaddr *)&dest_addr, dest_len) < 0) {
            printf("ERROR connecting to ssh server \n");
        }    
        else
            processSSHRequest(client_sock,server_sock,&total_bytes_received,&total_bytes_sent);
        close(client_sock);
        close(server_sock);
            
            //exit(1);
        
    }
    
    else if(port==21) {//ftp
        if (connect(server_sock, (struct sockaddr *)&dest_addr, dest_len) < 0) 
            printf("ERROR connecting to ftp server \n");
        else     
            processFTPRequest(client_sock,server_sock,&total_bytes_received,&total_bytes_sent);
        close(client_sock);
        close(server_sock);
            
            //exit(1);
        
    }
    
    else if(port==8080) {//my webserver
        printf("in http request\n");
        if (connect(server_sock, (struct sockaddr *)&dest_addr, dest_len) < 0)
            printf("ERROR connecting to http server \n");
        else
            processHTTPRequest(client_sock,server_sock,&total_bytes_received,&total_bytes_sent);
        close(client_sock);
        close(server_sock);
            
            //exit(1);
	}
    
    
    else { //invalid port number
        char * status = "Invalid Connection Port\n";
        send(client_sock,status,strlen(status),0);
        close(client_sock);
        close(server_sock);
        
        
    }
    
    
    //clean up
    snprintf(comm, sizeof(comm), "iptables -t nat -D POSTROUTING -p tcp -j SNAT --sport %hu --to-source %s", ntohs(proxy_addr.sin_port),inet_ntoa(client_addr.sin_addr));
    system(comm);
    
    FILE *fp = fopen("proxy.log","a");
    char log[2048];
    sprintf(log,"%s %s %hu %s %hu %lu %lu\n",str,inet_ntoa(client_addr.sin_addr),
            ntohs(client_addr.sin_port),inet_ntoa(dest_addr.sin_addr),ntohs(dest_addr.sin_port),
            total_bytes_sent,total_bytes_received);
    fwrite(log,sizeof(char),strlen(log),fp);
    fclose(fp);
    
    
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
        //FILE *fp = fopen("proxy.log","w"); //create a new one for each time running server
        //fclose(fp);
        run_server(atoi(argv[1]));
    }
    return 0;
}
