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

#include "helper.h"



/*void print_directories ()
{ //http://www.gnu.org/software/libc/manual/html_node/Simple-Directory-Lister.html
    DIR *dp;
    struct dirent *ep;
    
    dp = opendir ("./");
    if (dp != NULL)
    {
        while (ep = readdir (dp))
        puts (ep->d_name);
        (void) closedir (dp);
    }
    else
    perror ("Couldn't open the directory");
    
    return 0;
}*/

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
    /*while(strlen(buf)>2) {
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
         }
        //printf("the new buffer is: %s\n",buf);
        
    }*/
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
    HashTable *info = createHashTable(100);
    User u;
    
    int port=10000;
    
    strcpy(u.username,"Alex");
    strcpy(u.password,"password");
    int check = insert(info,u);
    printf("check: %d\n",check);
    Node * node = find(info,u);
    printf("Name: %s\n",(node->u).username);
    printf("Password: %s\n",(node->u).password);

            //printf("going around again: %d\n",i);
            readRequest(firstLine,host,connection,sock);
            User t;
            strcpy(t.username,firstLine);
            strcpy(t.password,"password");
    
            Node * node1 = find(info,u);
            printf("Name: %s\n",(node1->u).username);
            printf("Password: %s\n",(node1->u).password);
            send(sock, (node1->u).password, strlen((node1->u).password),0);
            
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

int main() {
  
    
    int port=10000;

    
    run_server(port);
}