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


#include <signal.h>


/* This flag controls termination of the main loop. */
volatile sig_atomic_t keep_going = 1;

/* The signal handler just clears the flag and re-enables itself. */
void
catch_alarm (int sig)
{
    keep_going = 0;
    signal (sig, catch_alarm);
}

char * contentType(char filename[]){
    char *content;
    char *token;
    char delim[2] = ".";
    if(strcmp(filename,"/")==0){
        return "Content-Type: text/html\r\n";
    }
    token = strtok(filename, delim);
    token = strtok(NULL, delim); //get what comes last. hack might have to fix
    if(strcmp(token,"html")==0 || strcmp(token,"htm")==0){
        content = "Content-Type: text/html\r\n";
    }
    else if(strcmp(token,"txt")==0) {
        content = "Content-Type: text/plain\r\n";
    }
    
    else if(strcmp(token,"gif")==0){
        content = "Content-Type: image/gif\r\n";
    }
    
    else if(strcmp(token,"png")==0){
        content = "Content-Type: image/png\r\n";
    }
    
    else if(strcmp(token,"jpg")==0){
        content = "Content-Type: image/jpg\r\n";
    }
    
    else {
        content = "";
    }
    
    return content;
    
}

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
    //printf("%s",firstLine);
}

void readRequest(char firstLine[], char host[], char connection[],int sock) {
    char buf[1024];
    char copy_buffer[1024];
    int len;
    char delim[2]=" ";
    readLine(buf, sock);
    strcpy(firstLine,buf);
    printf("the buffer is: %s\n",buf);
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
        //printf("the new buffer is: %s\n",buf);
        /*memset(buf,0,strlen(buf));
        printf("new buffer\n");
        printf("%s\n",buf);
        printf("cmp value: %d\n",strcmp(buf,"\n"));*/
    }
    //return keep_going;
    
}

void sendBinary(int sock,char * filename){
    char buf[1024];
    FILE *fp;

    long filesize = GetFileSize(filename);

    char contentHeader[100];
    sprintf(contentHeader,"Content-Length: %ld\r\n\r\n",filesize);
    send(sock,contentHeader,strlen(contentHeader),0);
    
    printf("File size: %s\n",contentHeader);

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
    printf("bytes read: %ld\n",total);
    fclose(fp);
}



void *processRequest(void *s) { //,char *document_root) {
    /* Establish a handler for SIGALRM signals. */
    //printf("first line read: %s\n", firstLine);
    //signal (SIGALRM, catch_alarm);
    
    /* Set an alarm to go off in a little while. */
    //alarm (10);
    
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
    char uri[100];
    char filename[1024];
    char *status1 = "HTTP/1.1 200 OK\r\n";
    //char *content1 = "Content-Type: text/html\r\n\r\n";
    int num = 0;
    char *token;
    char *default_file = "index.html";
    //char document_root[1024];
    char * document_root = "/Users/Alex/Downloads";
    int done;
    
    //Put this in a while loop?
    int i=0;

    
    while(1){
        FD_ZERO(&sockSet);
        FD_SET(sock,&sockSet);
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        
        selRet = select(sock+1,&sockSet,NULL,NULL,&tv);
        printf("fdset: %d\n",selRet);
        if(selRet==0) {//timeout :(
            printf("we timed out.\n");
            break;
        }
        
        else if(selRet==1){
        printf("going around again: %d\n",i);
        readRequest(firstLine,host,connection,sock);

        printf("finished reading request\n");
        //alarm(0);
        //alarm(10);
        
        //got something so let's process it, reset alarm
        //readFirstLine(firstLine, sock);
        
        printf("%s\n", firstLine);
        token = strtok(firstLine, delim);
        token = strtok(NULL, delim);
        //token = "index.html";
        
        //strcpy(filename, ".");
        printf("the current uri: %s\n",uri);
        strcat(uri, token);
        
        printf("Suffix: %s\n", uri);
        
        if(strcmp(uri,"/")==0){
            strcat(uri,default_file);
        }
        
        char forSuffix[100];
        strcpy(forSuffix,uri);
        char *content = contentType(forSuffix);
        
        strcpy(filename,document_root);
        strcat(filename,uri);
        
        printf("Full Path: %s\n",filename);
        printf("content type: %s\n",content);
        send(sock,status1,strlen(status1),0);
        send(sock,content,strlen(content),0);
        
        printf("This is the host: %s\n",host);
        printf("This is the connection: %s\n",connection);
        
        send(sock,connection,strlen(connection),0);
        
        sendBinary(sock,filename);
        printf("we sent the first part\n");
        
        //clean up memory for next part
        memset(firstLine,0,strlen(firstLine));
        memset(host,0,strlen(host));
        memset(connection,0,strlen(connection));
        memset(uri,0,strlen(uri));
        memset(forSuffix,0,strlen(forSuffix));
        printf("current uri: %s\n",uri);
        memset(filename,0,strlen(filename));
        i++;
        }
        else
            printf("ERROR\n");
    }
    printf("we want to close the socket\n");
    close(sock);
    /*
     if keep alive do not close the socket
     wait ten seconds for additional communication (how will I know this happened?)
    //add waiting here
    close(sock);
     */

    /*if(strcmp(filename,"/")==0){
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
        while (fgets(line, 1024, fp) != 0) {
            send(sock, line, strlen(line), 0);
        }

    }
        //here is where i should wait.
    
    
    close(sock);*/
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
        /*pid_t pid = fork();
        if(pid == 0) {//child process
           processRequest((void *) sent); //child will close process
        }*/
        pthread_create(&t,NULL,processRequest,(void *) sent);
        //close(cli); //parent doesn't need this
        
	}
    close(sock);
    
    
    
    
	
}

int main() {
    int port = 10000;
    char *document_root = "/Users/Alex/Desktop/Fall2015/Networks/www";
    run_server(port,document_root);
}
