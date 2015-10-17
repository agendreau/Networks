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
#include <netdb.h>

#include "helper.h"





//void run_client(int ports[])

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



/* Sends the file to the client byte by byte */
void sendBinary(int * sockets,char * filename){
    char buf[1024];
    FILE *fp;
    
    long filesize = GetFileSize(filename);
    
    long part_size = filesize/4;
    
    long extra = filesize%4;
    
    char contentHeader[100];

    long total = 0;
    int success;
    fp = fopen(filename,"rb"); //add error checking
    int add_byte;
    long bytes_to_read;
    size_t bytesRead;
    for(int i=0;i<4;i++){
        if(extra>0){
            add_byte=1;
            extra-=1;
        }
        else
            add_byte=0;
        bytes_to_read=part_size+add_byte;
        sprintf(contentHeader,"PUT %s %lu %d %lu\n",filename,bytes_to_read,i+1,total);
        send(sockets[i%2],contentHeader,strlen(contentHeader),0);
        int remaining = bytes_to_read%1024;
        fseek( fp, total, SEEK_SET );
        while(bytes_to_read/1024 > 0){
            bytesRead=fread( buf, sizeof(char), 1024, fp );
            total+=bytesRead;
            success = send(sockets[i%2], buf, bytesRead,0);
        }
        if(remaining>0){
            bytesRead=fread( buf, sizeof(char), remaining, fp );
            total+=bytesRead;
            success = send(sockets[i%2], buf, bytesRead,0);
        }
        
    }
    /*size_t bytesRead;
    while (((bytesRead=fread( buf, sizeof(char), bytes_to_read, fp )) > 0) && total<part_size+1) {
        total+=bytesRead;
        success = send(sock, buf, bytesRead,0);
    }*/
    
    //sleep(1);
    //printf("bytes read: %ld\n",total);
    fclose(fp);
}

void sendFile(int * sockets,char * filename){
    sendBinary(sockets,filename);
}

void receiveBinary(int sock,FILE * fp,char * filename, long filesize,
                   int part, int parts[],long offset){
    char buf[1024];
    char file[256];
    //sprintf(file,"DFS1/.%s.%d",filename,part);
    //printf("%s\n",file);
    
    ssize_t total_read_bytes=0;
    ssize_t read_bytes;
    
    fseek(fp,offset,SEEK_SET);
    
    int remaining = filesize%1024;
    
    while(filesize/1024 > 0){
        read_bytes = recv(sock, buf,1024,0);
        total_read_bytes+=read_bytes;
        fwrite(buf,sizeof(char),read_bytes,fp);
        //printf("total read bytes: %lu\n",total_read_bytes);
        //printf("read bytes: %lu\n",read_bytes);
    }
    if(remaining>0){
        read_bytes = recv(sock, buf,remaining,0);
        total_read_bytes+=read_bytes;
        fwrite(buf,sizeof(char),read_bytes,fp);
        //printf("total read bytes: %lu\n",total_read_bytes);
        //printf("read bytes: %lu\n",read_bytes);
        
    }
    
    parts[part-1]=1;
    //printf("parts[%d]: %d\n", part-1,parts[part-1]);
    
    //fclose(fp);

}

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

void processPart(int sock,int parts[], FILE *fp){
    
    char firstLine[1024];
    char request[8];
    char filename[100];
    long filesize;
    int part;
    long offset;
    char delim[2] = " ";
    char *token;
    
    readLine(firstLine,sock);
    printf("readline: %s\n",firstLine);
    
    
    printf("request: %s\n",firstLine);
    token = strtok(firstLine,delim);
    strcpy(request,token);
    token = strtok(NULL,delim);
    strcpy(filename,token);
    token= strtok(NULL,delim);
    filesize = atol(token);
    token=strtok(NULL,delim);
    part = atoi(token);
    token=strtok(NULL,delim);
    offset=atol(token);
    
    printf("filesize:%lu\n",filesize);
    printf("offset:%lu\n",offset);
    
    if(strcmp(request,"Part")==0)
        receiveBinary(sock,fp,filename,filesize,part,parts,offset);

}

void processGetRequest(int * sockets, char * filename) {
    struct timeval tv;
    fd_set sockSet;
    
    FD_ZERO(&sockSet);
    
    
    int parts[4];
    parts[0]=parts[1]=parts[2]=parts[3]=0;
    FILE *fp = fopen(filename,"wb");
    int j=0;
    int selRet = 0;
    //select statement;
    while(1){
        //reset timer each time around
        FD_ZERO(&sockSet);
        FD_SET(sockets[0],&sockSet);
        FD_SET(sockets[1],&sockSet);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        selRet = select(sockets[1]+1,&sockSet,NULL,NULL,&tv);
        if(selRet==0){
            printf("we timed out\n");
            fclose(fp);
            break;
        }
        
        else if(selRet==-1){
            printf("error in fd set\b");
            break;
        }
        else {//success
            for(int i=0;i<2;i++){
                if (FD_ISSET(sockets[i], &sockSet)) {
                    processPart(sockets[i],parts,fp);
                }
            }
        }
    }
    
    if(parts[0]==parts[1]==parts[2]==parts[3]!=1){
        printf("file incomplete\n");
    }
    

}

int communicate(int * sockets){
    char firstLine[1024];
    char request[8];
    char filename[100];
    long filesize;
    int part;
    long offset;
    char delim[2] = " ";
    char *token;
    int numServers=2;
    
    char contentHeader[100];
    long x = 0;
    
    int result = 0;
    
    char buffer[256];

    
    while(1){
        printf("Please enter the message: ");
        bzero(buffer,256);
        fgets(buffer,255,stdin);
        
        token=strtok(buffer,delim);
        strcpy(request,token);
        token=strtok(NULL,delim);
        if(token!=NULL)
            strcpy(filename,token);
        
        if(strcmp("GET",request)==0) {
            strtok(filename,"\n");
            printf("filename: %s\n",filename);
            sprintf(contentHeader,"GET %s %lu %d %lu\n",filename,x,0,x);
            for(int i=0;i<numServers;i++)
                send(sockets[i],contentHeader,strlen(contentHeader),0);
            processGetRequest(sockets,filename);
        }
        
        else if(strcmp("LIST",request)==0){
            printf("to be implemented\n");
        }
        
        
        else if(strcmp("PUT",request)==0){
            strtok(filename,"\n");
            sendFile(sockets,filename);
        }
        
        else if(strcmp("CLOSE",request)==0){
            sprintf(contentHeader,"CLOSE %s %lu %d %lu\n","test.txt",x,0,x);
            for(int i=0;i<numServers;i++){
                send(sockets[i],contentHeader,strlen(contentHeader),0);
                close(sockets[i]);
            }
            int result=1;
            break;
            
        }
        
        else {
            printf("bad command: shutting down\n");
            printf(contentHeader,"CLOSE %s %lu %d %lu\n","test.txt",x,0,x);
            for(int i=0;i<numServers;i++){
                send(sockets[i],contentHeader,strlen(contentHeader),0);
                close(sockets[i]);
            }
            int result=-1;
            break;
        }
        
    }
    return result;
}






int main(int argc, char *argv[]) {
    int sockfd, portno,portno2,portno3,portno4,n;
    struct sockaddr_in serv_addr;
    struct sockaddr_in serv_addr1;
    struct hostent *server;
    
    char contentHeader[100];
    long x = 0;

    
    char buffer[256];
    
    int numServers=2;
    
    int ports[4];
    
    int sockets[4];
    
    if (argc < 3) {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        exit(0);
    }
    
    
    
    ports[0] = atoi(argv[2]);
    ports[1]= 10002;
    ports[2] = 10003;
    ports[3] = 10004;
    
    /* Create a socket point */
    //sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockets[0] = socket(AF_INET, SOCK_STREAM, 0);
    sockets[1] = socket(AF_INET, SOCK_STREAM, 0);
    //sockets[2] = socket(AF_INET, SOCK_STREAM, 0);
    //sockets[3] = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sockets[0] < 0) {
        perror("ERROR opening socket");
        exit(1);
    }
    
    server = gethostbyname(argv[1]);
    
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    bzero((char *) &(serv_addr), sizeof(serv_addr));
    (serv_addr).sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&(serv_addr).sin_addr.s_addr, server->h_length);
    (serv_addr).sin_port = htons(ports[0]);
    
    /* Now connect to the server */
    if (connect(sockets[0], (struct sockaddr*)&(serv_addr), sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        exit(1);
    }
    
    bzero((char *) &(serv_addr1), sizeof(serv_addr1));
    (serv_addr1).sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&(serv_addr1).sin_addr.s_addr, server->h_length);
    (serv_addr1).sin_port = htons(ports[1]);
    
    /* Now connect to the server */
    if (connect(sockets[1], (struct sockaddr*)&(serv_addr1), sizeof(serv_addr1)) < 0) {
        perror("ERROR connecting");
        exit(1);
    }
    
    /* Now ask for a message from the user, this message
     * will be read by server
     */
    
    //Now check passwords
    sprintf(contentHeader,"User %s %lu %d %lu\n","Alex",x,0,x);
    for(int i=0;i<numServers;i++)
        send(sockets[i],contentHeader,strlen(contentHeader),0);
    
    printf("we want to send the password\n");
    sprintf(contentHeader,"Password %s %lu %d %lu\n","password",x,0,x);
    for(int j=0;j<numServers;j++){
        printf("Sending: %d\n",j);
        send(sockets[j],contentHeader,strlen(contentHeader),0);
    }
    
    for(int i=0;i<numServers;i++) {
        recv(sockets[i],buffer,256,0);
        if(strcmp(buffer,"0")==0){
            printf("Incorrect username/password\n");
            exit(-1);
        }
    }
    

    
    
    int good = communicate(sockets);
    
    if(good==1){
        printf("Everything went okay\n");
    }
    
    else if(good==-1){
        printf("There was an error\n");
    }
    

    return 0;
}