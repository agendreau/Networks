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
const char * default_files[3]; //assuming no more than 3 listed.

/* this is a bad hack for reading in valid file types */
int html=0;
int png=0;
int jpg=0;
int gif=0;
int css=0;
int js=0;
int txt=0;
int htm=0;

/*void lastThree(int s, const char *input,char ret[])
{
    From: http://www.dreamincode.net/forums/topic/162448-function-that-returns-last-three-characters-of-a-string/
    
    //char *ret = new char[s];
    
    for (int i = 0; i < s; i++)
    {
        ret[i] = input[strlen(input) - (s - i)];
    }
    ret[

    
    //return ret;
}*/


char * contentType(char filename[]){
    char *content;
    char *token;
    char delim[2] = ".";
    if(strcmp(filename,"/")==0){
        return "Content-Type: text/html\r\n";
    }
    token = strtok(filename, delim);
    token = strtok(NULL, delim); //get what comes last. hack might have to fix
    if((strcmp(token,"html")==0 && html) || (strcmp(token,"htm")==0 && htm)){
        content = "Content-Type: text/html\r\n";
    }
    else if(strcmp(token,"txt")==0 && txt) {
        content = "Content-Type: text/plain\r\n";
    }
    
    else if(strcmp(token,"gif")==0 && gif){
        content = "Content-Type: image/gif\r\n";
    }
    
    else if(strcmp(token,"png")==0 && png){
        content = "Content-Type: image/png\r\n";
    }
    
    else if(strcmp(token,"jpg")==0 && jpg){
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
    char status1[1024];
    //char * status1 = "HTTP/1.1 200 OK\r\n";

    
    //char *content1 = "Content-Type: text/html\r\n\r\n";
    int num = 0;
    char *token;
    char *default_file = "index.html";
    //char document_root[1024];
    //char * document_root = "/Users/Alex/Downloads";
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

        
        printf("%s\n", firstLine);
            char method[16];
        token = strtok(firstLine, delim);
            //token = "POST";
            strcpy(method,token);
            printf("compare methods: %d\n",strcmp(method,"GET"));
            
            /* make this a method */
            if(strcmp(method,"GET")){ //add lowercase?
                printf("bad method\n");
                sprintf(status1,"HTTP/1.1 400 Bad Request: Invalid Method: %s\r\n\r\n",method);
                send(sock,status1,strlen(status1),0);
                char * message = "Error 400 Bad Request: Invalid Method\n";
                send(sock,message,strlen(message),0);
                break;
            }
            
            
        token = strtok(NULL, delim); //filename (uri)
        //token = "index.html";
        
        //strcpy(filename, ".");
        printf("the current uri: %s\n",uri);
        strcat(uri, token);
        token = strtok(NULL, delim);
        char http_version[32];
            strncpy(http_version,token,8);
        //char delim_http[2] = "/";
        //char * version = strtok(http_portion,delim_http);
        printf("http version: %s\n",http_version);
        //version = strtok(NULL,delim_http);
            //version = (NULL,"/");
        
        //char version[3];
        //lastThree(3,http_portion,version);
        //printf("first token: %s\n",version);
        //char http_version = token[7];
        //check HTTP version
            /* make this a method*/
            printf("compare: %d\n",strcmp(http_version,"HTTP/1.1"));
            if(!(strcmp(http_version,"HTTP/1.1")==0 || strcmp(http_version,"HTTP/1.0")==0)){
                printf("bad status\n");
                sprintf(status1,"HTTP/1.1 400 Bad Request: Invalid HTTP-Version: %s\r\n\r\n",http_version);
                send(sock,status1,strlen(status1),0);
                char * message = "Error 400 Bad Request: Invalid HTTP-Version";
                send(sock,message,strlen(message),0);
                break;
            }
            else
               sprintf(status1,"%s 200 OK\r\n",http_version);
            
        //check method invalid if it isn't GET
            
        //char status1 = sprintf("HTTP/1.%d 200 OK\r\n",http_version-'0');
        printf("Suffix: %s\n", uri);
        //sprintf(status1,"HTTP/%s 200 OK\r\n","1.1");
        if(strcmp(uri,"/")==0){
            strcat(uri,default_file);
        }
        
        char forSuffix[100];
        strcpy(forSuffix,uri);
        char *content = contentType(forSuffix);
            
            if(strlen(content)<2){
                sprintf(status1,"￼￼HTTP/1.1 501 Not Implemented %s\r\n\r\n",uri);
                send(sock,status1,strlen(status1),0);
                char * message = "Error 501 Not Implemented\n";
                send(sock,message,strlen(message),0);
                break;
            }
        
        strcpy(filename,document_root);
        strcat(filename,uri);
            FILE * fp;
            fp = fopen(filename,"r");
            
            if(fp==NULL){
                printf("bad file\n");
                sprintf(status1,"￼HTTP/1.1 404 Not Found: %s\r\n\r\n",uri);
                send(sock,status1,strlen(status1),0);
                char * message = "Error 404 File Not Found\n";
                send(sock,message,strlen(message),0);
                break;
                
            }
            fclose(fp);
        
        printf("Full Path: %s\n",filename);
        printf("content type: %s\n",content);
        send(sock,status1,strlen(status1),0);
        send(sock,content,strlen(content),0);
        
        printf("This is the host: %s\n",host);
        printf("This is the connection: %s\n",connection);
        
        send(sock,connection,strlen(connection),0);
        
        sendBinary(sock,filename);
        printf("we sent the first part\n");
        
            if(strcmp(connection,"Connection: keep-alive")==0){
                break; //not a keep alive connection, done after one transfer
            }
        
        //clean up memory for next part, do I really need to do this?
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

    return NULL;
}


void run_server(int port)
{

	int sock, cli;
	struct sockaddr_in server,client;
	unsigned int len;
	//char mesg[] = "Hello to the world of socket programming";
	int *sent;
    pthread_t t;
    //port = 10000;

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
        //printf("client: %d\n",cli);
        sent = (int *) malloc(sizeof(int));
        *sent = cli;
        printf("here\n");
        //add pthread here
        /*pid_t pid = fork();
        if(pid == 0) {//child process
           processRequest((void *) sent); //child will close process
        }*/
        pthread_create(&t,NULL,processRequest,(void *) sent);
        //close(cli); //parent doesn't need this
        free(sent);
        
	}
    close(sock);
    
    
    
    
	
}

int main() {
    
    char line[1024]; //assuming lines in config file aren't that long (could make command line)
    //char document_root[1024]; //limiting size of document root, bad style
    //const char * default_files[3]; //assuming no more than 3 listed.
    char delim[2] = " ";
    char *token;
    int port;
    char delim_quote[2]="\"";
 
    
    FILE * config;
    config=fopen("ws.conf","r");
    while(fgets(line,1024,config)!=NULL){
        //printf("Line read: %s\n",line);
        token=strtok(line,delim);
        //printf("Token: %s\n",token);
        if(strcmp(token,"#serviceport")==0){
            //memset(line,0,strlen(line));
            fgets(line,1024,config);
            token=strtok(line,delim);
            token=strtok(NULL,delim);//get last entry (port number);
            port = atoi(token);
            printf("Port Number: %d\n",port);
        }
        else if(strcmp(token,"#document")==0){
            //memset(line,0,strlen(line));
            fgets(line,1024,config);
            token=strtok(line,delim);
            token=strtok(NULL,delim);
            token=strtok(token,delim_quote);
            strcpy(document_root,token);
            
            printf("Document root: %s\n",document_root);
        }
        else if(strcmp(token,"#default")==0){
            //memset(line,0,strlen(line));
            fgets(line,1024,config);
            token=strtok(line,delim);
            int i=0;
            while((token=strtok(NULL,delim))!=NULL){
                if(i>2){
                    printf("Too many default files.  Expecting max 3.\n");
                    exit(-1);
                }
                else{
                    
                    default_files[i]=token;
                    //printf("File token: %s\n",default_files[i]);
                    //not stripping newline from index.ws?
                    i++;
                }
            }
            
        }
        else if(strcmp(token,"#Content-Type")==0){
            //memset(line,0,strlen(line));
            printf("got to content type.\n");
            //fgets(line,1024,config);
            //printf("content line: %s\n",line);
            while(fgets(line,1024,config)!=NULL){
                if(strlen(line) < 2) break;
                //printf("content line: %s\n",line);
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
                //memset(line,0,strlen(line));
                printf("content line: %d\n",gif);
            }
           
        }
        else {
            printf("Invalid configuration file\n");
            //exit(-1);
        }
        //memset(line,0,strlen(line));

    }
    fclose(config);
    

    //exit(0);
    run_server(port);
}
