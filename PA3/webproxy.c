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


void readLine(char firstLine[], int sock) {
    int i = 0;
    char c;
    int len;
    len = recv(sock, &c, 1, 0);
    printf("read something: %d\n",len);
    while (len == 1 && c != '\n') {
        firstLine[i] = c;;
        i++;
        len = recv(sock, &c, 1, 0);
    }
    firstLine[i] = '\n';
    firstLine[i+1] = '\0';
    //trying to do some error handling, wasn't working very well
    /*if(len==-1){
     printf("error in read line\n");
     return 0;
     }
     else
     return 1;*/
    //printf("%s",firstLine);
}

void readRequestProxy(char firstLine[], char content_type[], char content_length[],int sock,char connection[]) {
    char buf[1024];
    char copy_buffer[1024];
    int len;
    char delim[2]=" ";
    readLine(buf,sock);
    //some error checking wasn't working
    /*if(readLine(buf, sock)==0){
     printf("error in read Request first line\n");
     return 0;
     }*/
    strcpy(firstLine,buf);
    printf("the buffer is: %s\n",buf);
    while(strlen(buf)>2) {
        char *token;
        strcpy(copy_buffer,buf);
        printf("the bufer is %s\n",buf);
        token = strtok(copy_buffer, delim);
        //printf("token: %s\n",token);
        if(strcmp(token,"Content-Type:")==0){
            strcpy(content_type,buf);
        }
        else if(strcmp(token,"Content-Length:")==0){
            strcpy(content_length,strtok(NULL,delim));
        }
        else if(strcmp(token,"Connection:")==0){
            strcpy(connection,buf);
        }
        readLine(buf,sock);
        //some error checking wasn't working
        /*if(readLine(buf, sock)==0){
         printf("error in read Request line\n");
         return 0;
         }*/
        //printf("the new buffer is: %s\n",buf);
        
    }
  
    //return 1;
    //return keep_going;
    
}

void communicate(int client_sock,int proxy_sock,char * request){
    printf("request\n");
    printf(request);
    //strcpy(request,"GET http://www.ascentrobotics.com/ HTTP/1.0\r\n\r\n");
    //char * request1 ="GET http://www.colorado.edu HTTP/1.0\r\n\r\n\r\n";
    //printf(request1);
    int j = send(proxy_sock,request,strlen(request),0);
    printf("request sent: %d\n",j);
    char firstLine[1024];
    char content_type[1024];
    char content_length[256];
    char content_length1[256];
    char connection[1024];
    char buf[1024];
    //sleep(5);
    readRequestProxy(firstLine,content_type,content_length,proxy_sock,connection);
    char * buffer;
    char * junk = "what is being sent\n";
    int c = atoi(content_length);
    buffer = malloc(sizeof(char)*atoi(content_length));
    printf("content length: %d\n",c);
    int i=0;
    sprintf(content_length1,"Content-Length: %s\r\n\r\n",content_length);
    printf(content_length1);
    send(client_sock,firstLine,strlen(firstLine),0);
    send(client_sock,content_type,strlen(content_type),0);
    send(client_sock,content_length1,strlen(content_length1),0);
    
    while ((i = recv(proxy_sock, buf, sizeof(buf),0))!= (size_t)NULL)//send file
    {
        //strcat(buffer,buf);
        send(client_sock,buf,strlen(buf),0);
    }
    //int i = recv(proxy_sock,buffer,c,0);
    //printf("recv: %d\n",i);
                 
            
    /*sprintf(content_length1,"Content-Length: %s\r\n\r\n",content_length);
    printf(content_length1);
    send(client_sock,firstLine,strlen(firstLine),0);
    send(client_sock,content_type,strlen(content_type),0);
    send(client_sock,content_length1,strlen(content_length1),0);
    //send(client_sock,connection,strlen(connection),0);
    send(client_sock,buffer,c,0);*/
    //send(client_sock,junk,strlen(junk),0);
    free(buffer);
    
    
}



void sendAndReceiverActual(int client_sock,char * host,char * request){
    int proxyClient;
    struct addrinfo hints, *res;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    char * port = "80";
    host[strlen(host)-2]='\0'; //get rid of the \r\n
    printf("actual host: %s\n",host);
    getaddrinfo(host, port, &hints, &res);
    printf("here");
   
    proxyClient = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    
 
    
    /* Now connect to the server */
    if (connect(proxyClient, res->ai_addr, res->ai_addrlen) < 0) {
        printf("ERROR connecting to server %d\n",port);
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
    //some error checking wasn't working
    /*if(readLine(buf, sock)==0){
     printf("error in read Request first line\n");
     return 0;
     }*/
    strcpy(firstLine,buf);
    strcat(request,buf);
    //printf("the buffer is: %s\n",buf);
    while(strlen(buf)>2) {
        char *token;
        strcpy(copy_buffer,buf);
        
        token = strtok(copy_buffer, delim);
        printf("token: %s\n",token);
        if(strcmp(token,"host:")==0 || strcmp(token,"HOST:")==0 || strcmp(token,"Host:")==0){
        
            token = strtok(NULL,delim);
            strcpy(host,token);
            printf("host: %s",host);
        }
    
        else if(strcmp(token,"Connection:")==0){
            strcpy(connection,buf);
        }
        readLine(buf,sock);
        strcat(request,buf);
        //some error checking wasn't working
        /*if(readLine(buf, sock)==0){
         printf("error in read Request line\n");
         return 0;
         }*/
        //printf("the new buffer is: %s\n",buf);
        
    }
    /*strcpy(host,"www.colorado.edu\r\n");*/
    //strcat(request,host);
    strcat(request,"\r\n");
    //return 1;
    //return keep_going;
    
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
   
    char request[4096];
    char firstLine[1024];
    char host[1024];
    char connection[1024];
    char delim[2] = " ";
    char crlf[3] = "\r\n";
    char uri[1024];
    char filename[1024];
    char status1[1024];
    char method[16];
    char forSuffix[1024];
    char content[100];
    char http_version[9];
    
    int num = 0;
    char *token;
    char default_file[33];
    int i=0;
    
    readRequest(firstLine,host,connection,sock,request);
    printf("host: %s\n",host);
    //check for errors
    
            
    token = strtok(firstLine, delim);
    //token = "POST"; //for testing bad method
    strcpy(method,token);
            
    /* Am I an GET method? */
    if(strcmp(method,"GET")){ //add lowercase?
        printf("Bad Method\n");
        sprintf(status1,"HTTP/1.0 400 Bad Request: Invalid Method: %s\r\n\r\n",method);
        send(sock,status1,strlen(status1),0);
        char * message = "Error 400 Bad Request: Invalid Method\n";
        send(sock,message,strlen(message),0);
        
    }
    
            
    /*token = strtok(NULL, delim); //filename (uri)
            
    //strcpy(uri, token); //
    //Am I a valid URI, my definition of a valid URI
    if(uri[0]!='/' || strlen(uri)>255){
        sprintf(status1,"HTTP/1.1 400 Bad Request: Invalid URI: %s\r\n\r\n",uri);
        printf("bad uri: %s\n",uri);
        send(sock,status1,strlen(status1),0);
        char * message = "Error 400 Bad Request: Invalid URI";
        send(sock,message,strlen(message),0);
        
    }
            token = strtok(NULL, delim);
            
            //token = "HTTP/1.12\r\n"; //to test bad version
            
             Am I a valid http version
    if(!((strcmp(token,"HTTP/1.0\n\n")==0|| strcmp(token,"HTTP/1.0\r\n")==0))){
        printf("Bad HTTP Version\n");
        sprintf(status1,"HTTP/1.0 400 Bad Request: Invalid HTTP-Version: %s\r\n\r\n",token);
        send(sock,status1,strlen(status1),0);
        char * message = "Error 400 Bad Request: Invalid HTTP-Version";
        send(sock,message,strlen(message),0);
    }
       
       
    strncpy(http_version,token,8); //assuming correct version, so this is okay
            
    //Parsed the message, now pass on to real server...
    */
    //printf(request);
    printf(host);
    sendAndReceiverActual(sock,host,request);
       
    /*
    strcpy(forSuffix,uri);
    contentType(content,forSuffix);
     Am I an implemented file type
    // Error sometimes has junk unknown why?
    if(strlen(content)<2){
        printf("Not Implemented\n");
        sprintf(status1,"￼￼HTTP/1.1 501 Not Implemented: %s\r\n\r\n",uri);
        send(sock,status1,strlen(status1),0);
        char * message = "Error 501 Not Implemented\n";
        send(sock,message,strlen(message),0);
    }*/
       
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
        perror("socket: ");
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
    
    while(1)
    {
        if((cli = accept(sock, (struct sockaddr *)&client,&len))==-1)
        {
            perror("accept");
            exit(-1);
        }
        
        sent = (int *) malloc(sizeof(int));
        *sent = cli;
        pthread_create(&t,NULL,processRequest,(void *) sent);
        free(sent);
        
    }
    close(sock);
    
}

int main(){
    run_server(8000);
    return 0;
}

