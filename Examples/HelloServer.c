#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>


void readFirstLine(char firstLine[], int sock) {
    int i = 0;
    char c;
    int len;
    len = recv(sock, &c, 1, 0);
    
    while (len == 1 && c != '\n') {
        firstLine[i] = c;;
        i++;
        len = recv(sock, &c, 1, 0);
    }
    firstLine[i] = '\n';
    firstLine[i+1] = '\0';
    printf("%s",firstLine);
}


void *processRequest(void *s,char *document_root) {
    int sock = *((int *) s);
    char firstLine[1024];
    char line[1024];
    char delim[2] = " ";
    char crlf[3] = "\r\n";
    char filename[100];
    char *status1 = "HTTP/1.0 200 OK\r\n";
    char *content1 = "Content-Type: text/html\r\n\r\n";
    int num = 0;
    char *token;
    readFirstLine(firstLine, sock);
    token = strtok(firstLine, delim);
    token = strtok(NULL, delim);
    strcpy(filename, ".");
    strcat(filename, token);
    printf("%s\n", filename);
    FILE *fp;
    if(strcmp(filename,"./")==0){
    //FILE *fp = fopen(filename, "r");
        //printf("here");
        //printf("%s",strcat(document_root,"index.html"));
        fp = fopen("/Users/Alex/Downloads/index.html", "r");
    }
    else {
        fp = fopen("/Users/Alex/Downloads/index.html","r");
        printf("oops\n");
    }
    if (fp == NULL) printf("Error\n");
    send(sock, status1, strlen(status1), 0);
    send(sock, content1, strlen(content1), 0);
    while (fgets(line, 1024, fp) != 0) {
        send(sock, line, strlen(line), 0);
    }
    close(sock);
    fclose(fp);
    
    return NULL;
}


void run_server(int port,char document_root[])
{

	int sock, cli;
	struct sockaddr_in server,client;
	unsigned int len;
	//char mesg[] = "Hello to the world of socket programming";
	int *sent;

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
        processRequest((void *) sent,document_root);

		close(cli);
	}
	
}

int main() {
    int port = 10000;
    char *document_root = "/Users/Alex/Desktop/Fall2015/Networks/www/";
    run_server(port,document_root);
}
