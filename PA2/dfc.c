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
void sendBinary(int sock,char * filename){
    char buf[1024];
    FILE *fp;
    
    long filesize = GetFileSize(filename);
    
    long part_size = filesize/4;
    
    long extra = filesize%4;
    
    char contentHeader[100];
    //sprintf(contentHeader,"PUT %s %ld %d\n","test.txt",filesize,1);
    //send(sock,contentHeader,strlen(contentHeader),0);
    
    //printf("File size: %s\n",contentHeader);
    
    /*char contentAlive[100];
     sprintf(contentAlive,"Connection: keep alive\r\n\r\n");
     send(sock,contentAlive,strlen(contentAlive),0);*/
    
    
    size_t total = 0;
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
        sprintf(contentHeader,"PUT %s %lu %d\n","test.txt",bytes_to_read,i+1);
        send(sock,contentHeader,strlen(contentHeader),0);
        int remaining = bytes_to_read%1024;
        fseek( fp, i*part_size, SEEK_SET );
        while(bytes_to_read/1024 > 0){
            bytesRead=fread( buf, sizeof(char), 1024, fp );
            success = send(sock, buf, bytesRead,0);
        }
        if(remaining>0){
            bytesRead=fread( buf, sizeof(char), remaining, fp );
            success = send(sock, buf, bytesRead,0);
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






int main(int argc, char *argv[]) {
    int sockfd, portno, portno2,portno3,portno4,n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    
    char buffer[256];
    
    if (argc < 3) {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        exit(0);
    }
    
    portno = atoi(argv[2]);
    /*portno2 = 10002;
    portno3 = 10003;
    portno4 = 10004;*/
    
    /* Create a socket point */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }
    
    server = gethostbyname(argv[1]);
    
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    
    /* Now connect to the server */
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        exit(1);
    }
    
    /* Now ask for a message from the user, this message
     * will be read by server
     */
    
    //printf("Please enter the message: ");
    //bzero(buffer,256);
    //fgets(buffer,255,stdin);
    
    sendBinary(sockfd,"testFiles/test.txt");
    
    /* Send message to the server */
    //n = write(sockfd, buffer, strlen(buffer));
    
    if (n < 0) {
        perror("ERROR writing to socket");
        exit(1);
    }
    
    /* Now read server response */
    bzero(buffer,256);
    n = read(sockfd, buffer, 255);
    
    if (n < 0) {
        perror("ERROR reading from socket");
        exit(1);
    }
    
    printf("%s\n",buffer);
    return 0;
}