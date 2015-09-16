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

char * contentType(char filename[]){
    char *content;
    char *token;
    char delim[2] = ".";
    if(strcmp(filename,"/")==0){
        return "Content-Type: text/html\r\n\r\n";
    }
    token = strtok(filename, delim);
    token = strtok(NULL, delim); //get what comes last. hack might have to fix
    if(strcmp(token,"html")==0 || strcmp(token,"htm")==0){
        content = "Content-Type: text/html\r\n\r\n";
    }
    else if(strcmp(token,"txt")==0) {
        content = "Content-Type: text/plain\r\n\r\n";
    }
    
    else if(strcmp(token,"gif")==0){
        content = "Content-Type: image/gif\r\n\r\n";
    }
    
    else if(strcmp(token,"jpg")==0){
        content = "Content-Type: image/jpg\r\n\r\n";
    }
    
    else {
        content = "";
    }
    
    return content;
    
}

long int GetFileSize(char* filename)
{ /*Got from 
   http://cboard.cprogramming.com/c-programming/79016-function-returns-number-bytes-file.html
   */
    long int size;
    FILE *f;
    
    f = fopen(filename, "rb");
    if (f == NULL) return -1;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fclose(f);
    
    return size;
}


void readLine(char firstLine[], int sock) {
    int i = 0;
    char c;
    int len;
    len = recv(sock, &c, 1, 0);
    printf("%c\n",c);
    while (len == 1 && c != '\n') {
        firstLine[i] = c;;
        i++;
        len = recv(sock, &c, 1, 0);
    }
    firstLine[i] = '\n';
    firstLine[i+1] = '\0';
    //printf("%s",firstLine);
}

bool readRequest(char firstLine[], char host[], int sock) {
    bool keep_alive = false;
    char buf[1024];
    int len;
    readLine(firstLine, sock);
    printf("first line read: %s\n", firstLine);
    //readLine(host,sock);
    //printf("%s",host);
    //read remaining bytes
    //readLine(buf,sock);
    /*while(1) {
        if(strlen(buf)>=10){
            char header[100];
            strncpy(header,buf,10);
            header[10]='\0';
            printf("header: %s\n",header);
            if(strcmp(header,"User-Agent")==0){
                break;
            }
            if(strcmp(header,"Connection")==0) {
                printf("got first comparison\n");
                char actual_header[100];
                strncpy(actual_header,buf,22);
                actual_header[22]='\0';
                if(strcmp(actual_header,"Connection: keep-alive")==0){
                    printf("connection found keep alive\n");
                    keep_alive = true;
                }
            }
        }
        memset(buf,0,strlen(buf));
        readLine(buf,sock);
        printf("new buffer\n");
        printf("%s\n",buf);
        printf("cmp value: %d\n",strcmp(buf,"\n"));
    }*/
    return keep_alive;
}

void sendBinary(int sock,char * filename){
    char buf[1024];
    FILE *fp;

    long int filesize = GetFileSize(filename);
    //long filesize = (long)(lseek(fp,(off_t)0,SEEK_END));
    //(void)lseek(fp,(off_t)0,SEEK_SET);
    //char contentHeader[100];
    //char * contentHeader = "Content-Length: 3536283\r\n";
    //sprintf(contentHeader,"Content-Length: %zu\r\n\r\n",filesize);
    //printf("content length: %s\n",contentHeader);
    
    //int x = send(sock,contentHeader,strlen(contentHeader),0);
    
    char contentAlive[100];
    //sprintf(contentAlive,"Connection: keep alive\r\n");
    //send(sock,contentAlive,strlen(contentAlive),0);
    //printf("content size: %d\n",x);
    size_t total = 0;
    //char new_buf[filesize];
    //bytesRead = fread(new_buf,sizeof(char),filesize,fp);
    //sendmsg(sock,new_buf,0);
    int success;
    fp = fopen(filename,"rb"); //add error checking
    size_t bytesRead;
    while (( bytesRead = fread( buf, sizeof(char), 1024, fp )) > 0 ) {
        total+=bytesRead;
        //printf("this is the buffer: %s\n",buf);
        success = send(sock, buf, bytesRead,0);
        //printf("did we send: %d\n",success);
        //printf("did we send: %zu\n",bytesRead);
        //memset(buf,0,strlen(buf));
        
    }
    sleep(1);
    //printf("size of char: %lu",sizeof(char));
    printf("bytes read: %ld\n",total);
    fclose(fp);
    //close(sock);
}



void *processRequest(void *s) { //,char *document_root) {
    int sock = *((int *) s);
    char firstLine[1024];
    char host[1024];
    char line[1024];
    char delim[2] = " ";
    char crlf[3] = "\r\n";
    char filename[100];
    char *status1 = "HTTP/1.0 200 OK\r\n";
    //char *content1 = "Content-Type: text/html\r\n\r\n";
    int num = 0;
    char *token;
    char *default_file = "/index.html";
    char document_root[1024];
    //document_root = "/Users/Alex/Desktop/Fall2015/Networks/www";
   
    
    bool keep_alive = readRequest(firstLine,host,sock);
    printf("finished reading request\n");
    //readFirstLine(firstLine, sock);
    printf("%s\n", firstLine);
    token = strtok(firstLine, delim);
    token = strtok(NULL, delim);
    //token = "index.html";
    
    //strcpy(filename, ".");
    strcat(filename, token);
    
    printf("%s\n", filename);
    char forSuffix[100];
    strcpy(forSuffix,filename);
    char *content = contentType(forSuffix);
    
    FILE *fp;
    printf("keep alive: %d\n",keep_alive);
    printf("%s\n",filename);
    if(strcmp(filename,"/")==0){
        printf("here1\n");
        char * index_file = "/Users/Alex/Downloads/index.html";
        //long imageSize = GetFileSize(index_file);
        //printf("size of image: %ld\n",imageSize);
        //fp = fopen(filename_png,"rb");
        //if (fp == NULL) printf("Error\n");
        send(sock, status1, strlen(status1), 0);
        char * contentHeader = "Content-Length: 3536283\r\n";
        //sprintf(contentHeader,"Content-Length: %zu\r\n\r\n",filesize);
        printf("content length: %s\n",contentHeader);
        
        int x = send(sock,contentHeader,strlen(contentHeader),0);
        //send(sock, content1, strlen(content1), 0);
        printf("content: %s\n",content);
        send(sock, content, strlen(content), 0);
        sendBinary(sock,index_file);
        /*while (fgets(line, 1024, fp) != 0) {
         send(sock, line, strlen(line), 0);
         }*/
        /*char filepath[1024];
        //strcpy(filepath,document_root);
        //strcat(filepath,"/index.html");
        fp = fopen("/Users/Alex/Desktop/Fall2015/Networks/www/index.html","r");
        if (fp == NULL) printf("Error\n");
        send(sock, status1, strlen(status1), 0);
        //send(sock, content1, strlen(content1), 0);
        printf("content: %s\n",content);
        send(sock, content, strlen(content), 0);
        while (fgets(line, 1024, fp) != 0) {
            send(sock, line, strlen(line), 0);
        }
        fclose(fp);*/
        

    }
    else {
        printf("here2\n");
        char * filename_png = "/Users/Alex/Desktop/Fall2015/Networks/www/images/meandz.jpg";
        //long imageSize = GetFileSize(filename_png);
        //printf("size of image: %ld\n",imageSize);
        //fp = fopen(filename_png,"rb");
        //if (fp == NULL) printf("Error\n");
        send(sock, status1, strlen(status1), 0);
        char * contentHeader = "Content-Length: 3536283\r\n";
        //sprintf(contentHeader,"Content-Length: %zu\r\n\r\n",filesize);
        printf("content length: %s\n",contentHeader);
        
        int x = send(sock,contentHeader,strlen(contentHeader),0);
        //send(sock, content1, strlen(content1), 0);
        printf("content: %s\n",content);
        send(sock, content, strlen(content), 0);
        sendBinary(sock,filename_png);
        /*while (fgets(line, 1024, fp) != 0) {
            send(sock, line, strlen(line), 0);
        }*/

    }
        //here is where i should wait.
    
    
    close(sock);
    return NULL;
}


void run_server(int port,char document_root[])
{

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
		
		//sent = send(cli, mesg, strlen(mesg),0);
		//printf("Sent %d bytes to client : %s\n",sent,inet_ntoa(client.sin_addr));
        *sent = cli;
        //add pthread here
        //processRequest((void *) sent);
        pthread_create(&t,NULL,processRequest,(void *) sent);
        
        
	}
    close(sock);
    
    
    
    
	
}

int main() {
    int port = 10000;
    char *document_root = "/Users/Alex/Desktop/Fall2015/Networks/www";
    run_server(port,document_root);
}
