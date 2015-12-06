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


/* globals for document root and default index files */
char document_root[1024]; //limiting size of document root, bad style
//const char * default_files[3]; //assuming no more than 3 listed.
char *default_files[10];
//totala number of default files
int dirIndexCount;

/* this is a bad hack for reading in valid file types */
/* Note I am only supporting html, htm, png, gif, txt, as per the assignment instructions*/
int html=0;
int png=0;
int jpg=0;
int gif=0;
int css=0;
int js=0;
int txt=0;
int htm=0;

/* Sends 500 Error */
void error_fivehundred(int sock){
    char * status1 = "HTTP/1.1 500 Internal Server Error: cannot allocate memory\r\n\r\n";
    send(sock,status1,strlen(status1),0);
    char * message = "Error 500 Internal Server Error: cannot allocate memory\n";
    send(sock,message,strlen(message),0);
    
}

/* Sets the content type of the file to be sent or teh empty string if it isn't support*/
void contentType(char content[],char filename[]){
    char *token;
    char delim[2] = ".";
    if(strcmp(filename,"")==0){
        strcpy(content,"");
        return;
    }
    if(strcmp(filename,"/")==0){
        strcpy(content,"Content-Type: text/html\r\n");
        return;
    }
    token = strtok(filename, delim);
    token = strtok(NULL, delim); //get what comes last. hack might have to fix
    if(token == NULL){
        strcpy(content,"");
        return;
    }
    if((strcmp(token,"html")==0 && html) || (strcmp(token,"htm")==0 && htm)){
        strcpy(content,"Content-Type: text/html\r\n");
    }
    else if(strcmp(token,"txt")==0 && txt) {
        strcpy(content,"Content-Type: text/plain\r\n");
    }
    
    else if(strcmp(token,"gif")==0 && gif){
        strcpy(content,"Content-Type: image/gif\r\n");
    }
    
    else if(strcmp(token,"png")==0 && png){
        strcpy(content,"Content-Type: image/png\r\n");
    }
    
    else if(strcmp(token,"jpg")==0 && jpg){
        strcpy(content,"Content-Type: image/jpg\r\n");
    }
    
    else {
        strcpy(content,"");
    }
    
    
}

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

/* reads message from socket character by character.  Lines can't have more than 1024 bytes*/
void readLine(char firstLine[], int sock) {
    int i = 0;
    char c;
    int len;
    len = recv(sock, &c, 1, 0);
    //printf("%c\n",c);
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

/* reads the client request
 * firstline gets the type of request (i.e. GET, POST, etc.)
 * host gets the host name
 * connection gets the connection type (i.e. keep-alive)
 * all other information from the request is igonred
 * the request end when the message from the socket has length 2 or less.
 */
void readRequest(char firstLine[], char host[], char connection[],int sock) {
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
    //printf("the buffer is: %s\n",buf);
    while(strlen(buf)>2) {
        char *token;
        strcpy(copy_buffer,buf);
        token = strtok(copy_buffer, delim);
        //printf("token: %s\n",token);
        if(strcmp(token,"host:")==0 || strcmp(token,"HOST:")==0){
            strcpy(host,buf);
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

/* Sends the file to the client byte by byte */
void sendBinary(int sock,char * filename){
    char buf[1024];
    FILE *fp;
    
    long filesize = GetFileSize(filename);
    
    char contentHeader[100];
    sprintf(contentHeader,"Content-Length: %ld\r\n\r\n",filesize);
    send(sock,contentHeader,strlen(contentHeader),0);
    
    //printf("File size: %s\n",contentHeader);
    
    /*char contentAlive[100];
     sprintf(contentAlive,"Connection: keep alive\r\n\r\n");
     send(sock,contentAlive,strlen(contentAlive),0);*/
    
    
    size_t total = 0;
    int success;
    fp = fopen(filename,"rb"); //add error checking
    size_t bytesRead;
    while (( bytesRead = fread( buf, sizeof(char), 1024, fp )) > 0 ) {
        total+=bytesRead;
        success = send(sock, buf, bytesRead,0);
    }
    
    //sleep(1);
    //printf("bytes read: %ld\n",total);
    fclose(fp);
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
    
    struct timeval tv;
    fd_set sockSet;
    
    FD_ZERO(&sockSet);
    
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
    int j=0;
    strcpy(default_file,"");
    /*search for valid default file*/
    FILE *test_index;
    for(j=0;j<dirIndexCount;j++){
        if((test_index = fopen(strcat(strcat(strdup(document_root),"/"),default_files[j]),"r"))!=NULL){
            strcpy(default_file,default_files[j]);
            break;
        }
    }
    //printf("default file: %s\n",default_file);
    
    
    
    /* receiving loop
     * continues to loop until timeout for keep-alive
     * breaks out of loop if connection = close
     */
    while(1){
        /*set up timer and socketSet for select statement*/
        //reset timer each time around
        FD_ZERO(&sockSet);
        FD_SET(sock,&sockSet);
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        
        selRet = select(sock+1,&sockSet,NULL,NULL,&tv);
        //printf("fdset: %d\n",selRet);
        //selRet=2; //for testing error 500
        if(selRet==0) {//timeout :(
            printf("we timed out.\n");
            break;
        }
        
        else if(selRet==1){
            //printf("going around again: %d\n",i);
            readRequest(firstLine,host,connection,sock);
            
            //some error checking, wasn't working
            /*if(readRequest(firstLine,host,connection,sock)==0){
             printf("error in reading request\n");
             error_fivehundred(sock);
             break;
             }*/
            
            //printf("finished reading request\n");
            
            token = strtok(firstLine, delim);
            //token = "POST"; //for testing bad method
            strcpy(method,token);
            
            /* Am I an GET method? */
            if(strcmp(method,"GET")){ //add lowercase?
                printf("Bad Method\n");
                sprintf(status1,"HTTP/1.1 400 Bad Request: Invalid Method: %s\r\n\r\n",method);
                send(sock,status1,strlen(status1),0);
                char * message = "Error 400 Bad Request: Invalid Method\n";
                send(sock,message,strlen(message),0);
                break;
            }
            
            
            token = strtok(NULL, delim); //filename (uri)
            
            strcpy(uri, token); //
            /* Am I a valid URI, my definition of a valid URI */
            if(uri[0]!='/' || strlen(uri)>255){
                sprintf(status1,"HTTP/1.1 400 Bad Request: Invalid URI: %s\r\n\r\n",uri);
                printf("bad uri: %s\n",uri);
                send(sock,status1,strlen(status1),0);
                char * message = "Error 400 Bad Request: Invalid URI";
                send(sock,message,strlen(message),0);
                break;
            }
            token = strtok(NULL, delim);
            
            //token = "HTTP/1.12\r\n"; //to test bad version
            
            /* Am I a valid http version */
            if(!(strcmp(token,"HTTP/1.1\n\n")==0 || strcmp(token,"HTTP/1.0\n\n")==0||
                 strcmp(token,"HTTP/1.1\r\n")==0 || strcmp(token,"HTTP/1.0\r\n")==0)){
                printf("Bad HTTP Version\n");
                sprintf(status1,"HTTP/1.1 400 Bad Request: Invalid HTTP-Version: %s\r\n\r\n",token);
                send(sock,status1,strlen(status1),0);
                char * message = "Error 400 Bad Request: Invalid HTTP-Version";
                send(sock,message,strlen(message),0);
                break;
            }
            
            strncpy(http_version,token,8); //assuming correct version, so this is okay
            
            /* if default setting it up*/
            if(strcmp(uri,"/")==0){
                if(strlen(default_file)>0){
                    strcat(uri,default_file);
                }
            }
            
            if(strcmp(uri,"/")==0){
                printf("no index file"); //throw 500 error here
                error_fivehundred(sock);
                break;
            }
            
            strcpy(forSuffix,uri);
            contentType(content,forSuffix);
            /* Am I an implemented file type */
            // Error sometimes has junk unknown why? */
            if(strlen(content)<2){
                printf("Not Implemented\n");
                sprintf(status1,"￼￼HTTP/1.1 501 Not Implemented: %s\r\n\r\n",uri);
                send(sock,status1,strlen(status1),0);
                char * message = "Error 501 Not Implemented\n";
                send(sock,message,strlen(message),0);
                break;
            }
            
            strcpy(filename,document_root);
            strcat(filename,uri);
            FILE * fp;
            fp = fopen(filename,"r");
            
            /* Can I find the file I want to load*/
            if(fp==NULL){
                printf("File not Found\n");
                sprintf(status1,"￼HTTP/1.1 404 Not Found: %s\r\n\r\n",uri);
                send(sock,status1,strlen(status1),0);
                char * message = "Error 404 File Not Found\n";
                send(sock,message,strlen(message),0);
                break;
                
            }
            fclose(fp);
            
            /*printf("Full Path: %s\n",filename);
            printf("content type: %s\n",content);
            printf("This is the host: %s\n",host);
            printf("This is the connection: %s\n",connection);*/
            
            /* all errors passed, should be okay to send file*/
            sprintf(status1,"%s 200 OK\r\n",http_version);
            send(sock,status1,strlen(status1),0);
            send(sock,content,strlen(content),0);
            send(sock,connection,strlen(connection),0);
            
            sendBinary(sock,filename);


            /* Do I want to keep the connection alive?*/
            if(strcmp(connection,"Connection: keep-alive\r\n")){
                printf("connection: %s\n",connection);
                break; //not a keep alive connection, done after one transfer
            }
            
            i++; //times around loop
        }
        else { //select failed
            printf("ERROR\n");
            error_fivehundred(sock);
            break;
        }
    }
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
        //pthread_create(&t,NULL,processRequest,(void *) sent);
        processRequest((void *) sent);
        free(sent);
        
    }
    close(sock);
    
    
    
    
    
}

int main() {
    
    char line[1024]; //assuming lines in config file aren't that long (could make command line)
    char delim[2] = " ";
    char *token;
    int port;
    char delim_quote[2]="\"";
    
    
    FILE * config;
    config=fopen("ws.conf","r"); //assuming in same directory as server
    /* parse config file */
    while(fgets(line,1024,config)!=NULL){
        token=strtok(line,delim);
        if(strcmp(token,"#serviceport")==0){
            fgets(line,1024,config);
            token=strtok(line,delim);
            token=strtok(NULL,delim);//get last entry (port number);
            port = atoi(token);
            //printf("Port Number: %d\n",port);
        }
        else if(strcmp(token,"#document")==0){
            fgets(line,1024,config);
            token=strtok(line,delim);
            token=strtok(NULL,delim);
            token=strtok(token,delim_quote);
            strcpy(document_root,token);
            
            //printf("Document root: %s\n",document_root);
        }
        else if(strcmp(token,"#default")==0){
            fgets(line,1024,config);
            dirIndexCount = 0;
            //printf("line: %s\n",line);
            token = strtok(line, " \t");
            token = strtok(NULL, " \t");
            while (token != NULL) {
                default_files[dirIndexCount] = strdup(token);
                dirIndexCount++;
                token= strtok(NULL, " \t");
                //printf("new token: %s\n",token);
            }
            default_files[dirIndexCount-1]=strtok(default_files[dirIndexCount-1],"\n");
            /*for (int i = 0; i < dirIndexCount; i++)
                printf("index file %d: %s\n", i, default_files[i]);*/
            
        }
        else if(strcmp(token,"#Content-Type")==0){
            //printf("got to content type.\n");
            while(fgets(line,1024,config)!=NULL){
                if(strlen(line) < 2) break;
                token=strtok(line,delim);
                if(strcmp(token,".html")==0) html=1;
                else if(strcmp(token,".png")==0) png=1;
                else if(strcmp(token,".gif")==0) gif=1;
                else if(strcmp(token,".jpg")==0) jpg=1;
                else if(strcmp(token,".css")==0) css=1;
                else if(strcmp(token,".js")==0) js=1;
                else if(strcmp(token,".txt")==0) txt=1;
                else if(strcmp(token,".htm")==0) htm=1;
                else {
                    printf("Unknown file type.\n");
                    exit(-1);
                }
                
            }
            
        }
        
    }
    fclose(config);
    
    run_server(port);
}
