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
void communicate(int client_sock,int proxy_sock,char * request,char * url, long content_length){
    
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
        return;
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
            return;
        }
        else if(response_content==-2){
            printf("error in receiving request from server\n");
            char * status = "HTTP/1.0 400 Bad Request: Invalid Content Length: \r\n\r\n";
            send(client_sock,status,strlen(status),0);
            return;
        }
        else {
            //printf("we made it here\n");
            bytes_sent=send(client_sock,response,strlen(response),0);
            if(bytes_sent<=0){
                printf("error in sending response to client\n");
                return;
            }
            else {
                total_read_bytes=0;
                while(total_read_bytes<response_content){
                    read_bytes = recv(proxy_sock, buf,1024,0);//send file
                    if(read_bytes<=0){
                        printf("error in reading file from server\n");
                        return;
                    }
                    total_read_bytes+=read_bytes;
                    bytes_sent = send(client_sock,buf,read_bytes,0);
                    if(bytes_sent<=0){
                        printf("error in sending file to client\n");
                        return;
                    }
                }
                
                //printf("out of while\n");
            }
        }
    }
    
   
    
}


/* Set up the socket to communicate between the proxy and the server
 * This will segfault if the server is not running on the port
 */
void sendAndReceiverActual(int client_sock,char * host,char * request,
                           char * port,char * uri,long content_length){
    int proxyClient;
    struct addrinfo hints, *res;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    //char * port = "80";
    //host[strlen(host)-2]='\0'; //get rid of the \r\n
    //printf("Actual host: %s\n",host);
    
    getaddrinfo(host, port, &hints, &res);
    
    //seg fault if server isn't running (i.e. get address info of inactive port)
    proxyClient = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    
    
    
    /* Now connect to the server */
    if (connect(proxyClient, res->ai_addr, res->ai_addrlen) < 0) {
        printf("ERROR connecting to server %s\n",port);
        //exit(1);
    }
    
    //Communicate between the server and the client
    communicate(client_sock,proxyClient,request,uri,content_length);
    close(proxyClient);
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
    
    //Setting everything up.  Not the best way to manage my memory
    int sock = *((int *) s);
    
    free(s);//free memore once I have placed it in the int s
    
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
    
    
    
    int content_length = readRequest(sock,request);
    
    printf("Request\n%s\n",request);
    //check for errors
    if(content_length==-1){
        printf("we want to close the socket on an empty request call\n");
        close(sock);
        return NULL;
    }
    
    
    /* Am I an GET method? */
    if(strncmp(request,"GET",3)!=0 &&
       strncmp(request,"get",3)!=0 &&
       strncmp(request,"Get",3)!=0){
        printf("Bad Method\n");
        sprintf(status1,"HTTP/1.0 400 Bad Request: Invalid Method: Not Get\r\n\r\n");
        send(sock,status1,strlen(status1),0);
        printf("we want to close the socket on bad method request\n");
        close(sock);
        
        return NULL;
        //break;
    }
    
    
    char * url_start = strstr(request,"GET http://");
    char * version = strstr(request,"HTTP/1.0");
    
    /* Am I a URI, kinda check*/
    if(url_start==NULL){
        printf("Bad URI\n");
        sprintf(status1,"HTTP/1.0 400 Bad Request: Invalid URI: \r\n\r\n");
        send(sock,status1,strlen(status1),0);
        printf("we want to close the socket on bad uri request\n");
        close(sock);
        return NULL;
    }
    
    /*Am I a valid HTTP vesrion*/
    if(version==NULL){
        printf("Invalid HTTP\n");
        sprintf(status1,"HTTP/1.0 400 Bad Request: Invalid HTTP-Version: Not 1.0 \r\n\r\n");
        send(sock,status1,strlen(status1),0);
        printf("we want to close the socket on bad http request\n");
        close(sock);
        return NULL;
    }
    
    /*extract host or host:port*/
    int i=11;
    while(url_start[i]!='/'){
        host[i-11]=url_start[i];
        i++;
    }
    
    /*extract uri*/
    i=4;
    while(url_start[i]!=' '){
        uri[i-4]=url_start[i];
        i++;
    }
    
    /*parse host/port if one listed, or set default port to 80*/
    char * port_start = strstr(host,":");
    int j;
    int k;
    if(port_start!=NULL){
        for(j=1;j<strlen(port_start);j++){
            port[j-1]=port_start[j];
        }
        for(k=0;k<strlen(host);k++){
            if(host[k]==':') {
                host[k]='\0';
                break;
            }
        }
    }
    else {//no port specified
        
        port[0]='8';
        port[1]='0';
        port[2]='\0';
        
    }
    
    /* Some checks*/
    printf("host: %s\n",host);
    printf("port: %s\n",port);
    printf("uri: %s\n",uri);
    printf("Request\n%s\n",request);
    
    //SET PORT
    port = 8080;
    sendAndReceiverActual(sock,host,request,port,uri,content_length);
    printf("we want to close the socket\n");
    close(sock);
    
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
        //run_server(atoi(argv[1]));
        run_server(8000);
    }
    return 0;
}

