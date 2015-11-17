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



/* returns the number of bytes in a file*/
long GetFileSize(char* filename)
{ /*Got from
   http://cboard.cprogramming.com/c-programming/79016-function-returns-number-bytes-file.html
   */
    long size;
    FILE *f;
    
    f = fopen(filename, "rb");
    if (f == NULL) return -1;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fclose(f);
    
    return size;
}

int readLine(char firstLine[], int sock) {
    int i = 0;
    char c;
    int len;
    len = recv(sock, &c, 1, 0);
    printf("len: %d\n",len);
    while (len == 1 && c != '\n') {
        firstLine[i] = c;;
        i++;
        len = recv(sock, &c, 1, 0);
    }
    firstLine[i] = '\n';
    firstLine[i+1] = '\0';
    printf("readLine: %s",firstLine);
    return len;
 
}

long readRequestProxy(int server_sock, int client_sock,char response[]) {
    char buf[1024];
    char copy_buffer[1024];
    long content_length;
    char delim[2]=" ";
    readLine(buf,server_sock);
    //strcpy(firstLine,buf);
    send(client_sock,buf,strlen(buf),0);
    printf("the buffer is: %s\n",buf);
    strcat(response,buf);
    char header[1024];
    
    int open;
    
    while(1) {
        strcat(response,buf);
        char *token;
        
        strcpy(copy_buffer,buf);
        printf("the buffer is %s\n",buf);
        
        strcpy(strtok_r(copy_buffer, delim,&token);
       
        if(strcmp(header,"Content-Length:")==0){
            content_length = atol(strtok_r(NULL,delim,&token));
        }
        
        open = readLine(buf,server_sock);
        printf("open response: %d\n",open);
        
        if(strcmp(buf,"\r\n")==0 || open<=0){// &&request[rl-1]=='\n' && request[rl-2]=='\r'){
            if(open<=0){
                printf("here in response\n");
                break;
            }
            else {
            send(client_sock,buf,strlen(buf),0);
            strcat(response,buf);
            break;
            }
        }
        
        strcat(response,buf);
        
        
        
    }
    
  
    return content_length;
    
}

void communicate(int client_sock,int proxy_sock,char * request){
    //printf("request\n");
    //printf(request);
    //strcpy(request,"GET http://www.ascentrobotics.com/ HTTP/1.0\r\n\r\n");
    //char * request1 ="GET http://www.colorado.edu HTTP/1.0\r\n\r\n\r\n";
    //printf(request1);
    int j = send(proxy_sock,request,strlen(request),0);
    printf("request sent: %d\n",j);
    char response[4096];
    response[0]='\0';
    char buf[1024];
    
    long content_length = readRequestProxy(proxy_sock,client_sock,response);
    
    ssize_t total_read_bytes=0;
    ssize_t read_bytes;
    ssize_t bytes_sent;
    printf("we made it here\n");
    
    while(total_read_bytes<content_length){
        read_bytes = recv(proxy_sock, buf,1024,0);//send file
        total_read_bytes+=read_bytes;
        bytes_sent = send(client_sock,buf,read_bytes,0);
    }
    
    printf("out of while\n");

}



void sendAndReceiverActual(int client_sock,char * host,char * request){
    int proxyClient;
    struct addrinfo hints, *res;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    char * port = "80";
    host[strlen(host)-2]='\0'; //get rid of the \r\n
    printf("Actual host: %s\n",host);
    getaddrinfo(host, port, &hints, &res);
   
    proxyClient = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    
 
    
    /* Now connect to the server */
    if (connect(proxyClient, res->ai_addr, res->ai_addrlen) < 0) {
        printf("ERROR connecting to server %s\n",port);
        //exit(1);
    }
    
    communicate(client_sock,proxyClient,request);
    close(proxyClient);
}



/* reads the client request
 * firstline gets the type of request (i.e. GET, POST, etc.)
 * host gets the host name
 * connection gets the connection type (i.e. keep-alive)
 * all other information from the request is igonred
 * the request end when the message from the socket has length 2 or less.
 */
void readRequest(char firstLine[], char host[], char connection[],int sock,char request[]) {
    char buf[1024];
    
    char copy_buffer[1024];
    int len;
    char delim[2]=" ";
    readLine(buf,sock);
    strcpy(firstLine,buf);
    strcat(request,buf);
    //printf("the buffer is: %s\n",buf);
    int rl = 0;
    rl+=strlen(buf);
    int open;
    while(1) {
        char *token;
        char *header;
        strcpy(copy_buffer,buf);
        
        rl+=strlen(buf);
        //token = strtok_r(copy_buffer, delim,&token);
        header = strtok_r(copy_buffer, delim,&token);
        printf("token: %s\n",header);
        if(strcmp(header,"host:")==0 || strcmp(header,"HOST:")==0 || strcmp(header,"Host:")==0){
            strcpy(host,strtok_r(NULL, delim,&token));
            //printf("host: %s",host);
        }
    
        else if(strcmp(header,"Connection:")==0){
            strcpy(connection,strtok_r(NULL, delim,&token));
        }
        open = readLine(buf,sock);
        //printf("buf2: %d\n",strcmp(&request[rl-1],"\n"));
        //printf("buf1: %d\n",strcmp(&request[rl-2],"\r"));
        printf("buf: %d\n",(strcmp(buf,"\r\n")));
        printf("open request: %d\n",open);
        if(strcmp(buf,"\r\n")==0 || open<=0){// &&request[rl-1]=='\n' && request[rl-2]=='\r'){
            if(open<=0){
                printf("here in request\n");
                break;
            }
            else {
            strcat(request,buf);
            break;
            }
        }
        
        strcat(request,buf);
        
    }
    
}


/* Processes the client request
 * Reads the request
 * Sets up the select for pipelining when keep-alive connection
 * Checks for errors in request and sends back the correct error message
 * closes the connection once we have been waiting for 10 seconds (keep-alive)
 * closes the connection immediately after message sent (close)
 */

void *processRequest(void *s) { //,char *document_root) {
    
    //Setting everything up.  Not the best way to manage my memory
    int sock = *((int *) s);
    int selRet;
    
    /*struct timeval tv;
    fd_set sockSet;
    
    FD_ZERO(&sockSet);*/


   
    
    
    //while(1){
        char request[4096];
        request[0]='\0';
        char firstLine[1024];
        char host[128];
        host[0]='\0';
        char connection[32];
        char delim[2] = " ";
        char crlf[3] = "\r\n";
        char uri[1024];
        char status1[1024];
        char method[16];
        
        int num = 0;
        char *token;
        char default_file[33];
        int i=0;
        /*set up timer and socketSet for select statement*/
        //reset timer each time around
        /*FD_ZERO(&sockSet);
        FD_SET(sock,&sockSet);
        tv.tv_sec = 20;
        tv.tv_usec = 0;
        
        selRet = select(sock+1,&sockSet,NULL,NULL,&tv);
        //printf("fdset: %d\n",selRet);*/
        //selRet=2; //for testing error 500
        /*if(selRet==0) {//timeout :(
           printf("we timed out.\n");
            break;
        }
        
        else if(selRet==1){*/
    
  

    
    readRequest(firstLine,host,connection,sock,request);
    printf("firstline: %s\n",firstLine);
    printf("host: %s\n",host);
    printf("connection: %s\n",connection);
    printf("Request\n%s\n",request);
    //check for errors
    
   
    
    strcpy(method,strtok_r(firstLine, delim,&token));
    printf("Method: %s\n",method);
    if(strlen(host)==0){
        strcpy(host,"cs.wellesley.edu\r\n");
    }
            
    /* Am I an GET method? */
    if(strcmp(method,"GET")){ //add lowercase?
        printf("Bad Method\n");
        sprintf(status1,"HTTP/1.0 400 Bad Request: Invalid Method: %s\r\n\r\n",method);
        send(sock,status1,strlen(status1),0);
        //char * message = "Error 400 Bad Request: Invalid Method\n";
        //send(sock,message,strlen(message),0);
        //close(sock);
        printf("we want to close the socket on bad request\n");
        close(sock);
        
        return NULL;
        //break;
        
        
    }
    else {
    
    
    sendAndReceiverActual(sock,host,request);
       
    }
    
    //    }

    //all done so we want to close the socket
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
    int i = 0;
    while(1)
    {
        if((cli = accept(sock, (struct sockaddr *)&client,&len))==-1)
        {
            printf("accept\n");
            exit(-1);
        }
        i++;
        printf("DUMB: %d\n",i);
        sent = (int *) malloc(sizeof(int));
        *sent = cli;
        //pthread_create(&t,NULL,processRequest,(void *) sent);
        processRequest((void *) sent);
        free(sent);
        
    }
    close(sock);
    
}

int main(){
    run_server(8000);
    return 0;
}

