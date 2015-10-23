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
#include <openssl/md5.h>



int numServers=4;
int maxSocket;
char password[256];

//http://www.sparknotes.com/cs/searching/hashtables/section3/page/2/

typedef struct {
    char filename[256];
    int part[4];
} File_;

struct FileList {
    File_ f;
    struct FileList *next;
};

struct FileList;
typedef struct FileList FileList;

typedef struct {
    int size;
    FileList **table;
} HashFileTable;


HashFileTable *createHashTable(int size)
{
    HashFileTable *newTable;
    
    if (size<1) return NULL; /* invalid size for table */
    
    /* Attempt to allocate memory for the table structure */
    if ((newTable = malloc(sizeof(unsigned int))) == NULL) {
        return NULL;
    }
    
    /* Attempt to allocate memory for the table itself */
    if ((newTable->table = malloc(sizeof(FileList *) * size)) == NULL) {
        return NULL;
    }
    
    /* Initialize the elements of the table */
    for(int i=0; i<size; i++) newTable->table[i] = NULL;
    
    /* Set the table's size */
    newTable->size = size;
    
    return newTable;
}

unsigned int hashValue(File_ file,HashFileTable *ht){
    unsigned int hv = strlen(file.filename) % (ht->size);
    //printf("hash value: %u\n",hv);
    return hv;
}

FileList * find(HashFileTable *ht,File_ file){
    unsigned int hv = hashValue(file,ht);
    FileList *node;
    for(node=ht->table[hv];node!=NULL;node=node->next){
        if(strcmp(file.filename,(node->f).filename)==0)
        return node;
    }
    return NULL;
    
}

int insert(HashFileTable *ht,File_ file){
    FileList *newNode;
    FileList *currentNode;
    unsigned int hv = hashValue(file,ht);
    
    if ((newNode = malloc(sizeof(FileList))) == NULL) return 1; //failed to allocate memory
    
    currentNode = find(ht,file);
    if(currentNode!=NULL) return 2; //already exists
    //newNode->User = malloc(sizeof(User));
    newNode->f = file;
    newNode->next = ht->table[hv];
    ht->table[hv]=newNode;
    //printf("Name: %s\n",(ht->table[hv]->u).username);
    //printf("Password: %s\n",(ht->table[hv]->u).password);
    return 0;
}

void free_table(HashFileTable *ht)
{
    int i;
    FileList *node, *temp;
    
    if (ht==NULL) return;
    
    /* Free the memory for every item in the table, including the
     * strings themselves.
     */
    for(i=0; i<ht->size; i++) {
        node = ht->table[i];
        while(node!=NULL) {
            temp = node;
            node = node->next;
            //free(temp->u);
            free(temp);
        }
    }
    
    /* Free the table itself */
    free(ht->table);
    free(ht);
}




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




int calculateHash(char * filename) {
    /*From here: http://stackoverflow.com/questions/10324611/how-to-calculate-the-md5-hash-of-a-large-file-in-c*/
    unsigned char c[MD5_DIGEST_LENGTH]; //this is 32
    int i;
    FILE *inFile = fopen (filename, "rb");
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];
    char *ptr;
    
    int mod=0;
    unsigned int decimal;
    
    if (inFile == NULL) {
        printf ("%s can't be opened.\n", filename);
        return 0;
    }
    
    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, inFile)) != 0)
    MD5_Update (&mdContext, data, bytes);
    MD5_Final (c,&mdContext);
    char part[3];
    for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
        printf("%02x ", c[i]);
    }
    sprintf(part,"%02x",c[0]);
    int hash = strtol(part,&ptr,16);

    printf("last part: %d\n",hash);
    printf (" %s\n", filename);

    fclose (inFile);
    return hash%4;
    //return 0;
    
}

long sendPart(int sock1, int sock2,long bytes_to_read, FILE *fp, long total) {
    char buf[1024];
    int remaining = bytes_to_read%1024;
    int success;
    size_t bytesRead;
    long total_read=0;
    fseek( fp, total, SEEK_SET );
    while(bytes_to_read/1024 > 0){
        bytesRead=fread( buf, sizeof(char), 1024, fp );
        total_read+=bytesRead;
        bytes_to_read=bytes_to_read-bytesRead;
        success = send(sock1, buf, bytesRead,0);
        success = send(sock2, buf, bytesRead,0);
        
    }
    if(remaining>0){
        bytesRead=fread( buf, sizeof(char), remaining, fp );
        total_read+=bytesRead;
        bytes_to_read=bytes_to_read-bytesRead;
        success = send(sock1, buf, bytesRead,0);
        success = send(sock2, buf, bytesRead,0);
    }
    return total+total_read;
    

}

void sendHash(int * sockets, int toSend[4][2],FILE *fp,long part_size,
              long extra,char * filename, char * directory) {
    long bytes_to_read;
    long add_byte=0;
    char contentHeader[100];
    long total=0;
    for(int i=0;i<4;i++){
        if(extra>0){
            add_byte=1;
            extra-=1;
        }
        else
        add_byte=0;
        bytes_to_read=part_size+add_byte;
        sprintf(contentHeader,"PUT %s %s %lu %d %lu\n",filename,directory,bytes_to_read,i+1,total);
        
        send(sockets[toSend[i][0]],contentHeader,strlen(contentHeader),0);
        send(sockets[toSend[i][1]],contentHeader,strlen(contentHeader),0);
        total = sendPart(sockets[toSend[i][0]],sockets[toSend[i][1]],bytes_to_read,fp,total);
        
    }
}

/* Sends the file to the client byte by byte */
void sendBinary(int * sockets,char * filename, char * directory){
    
    FILE *fp;
    
    
    
    
    char dir[1024];
    char dir1[2014];
    char secure_file[256];
    if(strcmp("/",directory)==0){
        sprintf(dir,"%s.%s","",filename);
        sprintf(dir1,"%s%s","",filename);
    }
    else {
    sprintf(dir,"%s.%s",directory,filename);
    sprintf(dir1,"%s%s",directory,filename);
    }
    char command[1024];
    
    sprintf(command,"openssl base64 -in %s -out %s -k %s",dir1,dir,password);
    
    system(command);
    
    int hash = calculateHash(dir);
    
    long filesize = GetFileSize(dir);
    
    long part_size = filesize/4;
    
    long extra = filesize%4;

    

    fp = fopen(dir,"rb"); //add error checking
   
    int toSend[4][2];
    switch (hash) {
        case 0:
            toSend[0][0]=0;
            toSend[0][1]=3;
            toSend[1][0]=0;
            toSend[1][1]=1;
            toSend[2][0]=1;
            toSend[2][1]=2;
            toSend[3][0]=2;
            toSend[3][1]=3;
            sendHash(sockets,toSend,fp,part_size,extra,filename,directory);
            break;
        
        case 1:
            toSend[0][0]=0;
            toSend[0][1]=1;
            toSend[1][0]=1;
            toSend[1][1]=2;
            toSend[2][0]=2;
            toSend[2][1]=3;
            toSend[3][0]=0;
            toSend[3][1]=3;
            sendHash(sockets,toSend,fp,part_size,extra,filename,directory);
            break;
            
        case 2:
            toSend[0][0]=1;
            toSend[0][1]=2;
            toSend[1][0]=2;
            toSend[1][1]=3;
            toSend[2][0]=0;
            toSend[2][1]=3;
            toSend[3][0]=0;
            toSend[3][1]=1;
            sendHash(sockets,toSend,fp,part_size,extra,filename,directory);
            break;
            
        case 3:
            toSend[0][0]=2;
            toSend[0][1]=3;
            toSend[1][0]=0;
            toSend[1][1]=3;
            toSend[2][0]=0;
            toSend[2][1]=1;
            toSend[3][0]=1;
            toSend[3][1]=2;
            sendHash(sockets,toSend,fp,part_size,extra,filename,directory);
            break;
        
    }
    fclose(fp);
}

void sendFile(int * sockets,char * filename,char * directory){
    sendBinary(sockets,filename,directory);
}

void receiveBinary(int sock,FILE * fp, long filesize,
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
        filesize=filesize-read_bytes;
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
    printf("firstline from socket(%d): %s\n",sock,firstLine);
    //trying to do some error handling, wasn't working very well
    /*if(len==-1){
     printf("error in read line\n");
     return 0;
     }
     else
     return 1;*/
    //printf("%s",firstLine);
}

void readList(char firstLine[], int sock) {
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
    printf("firstline from socket(%d): %s\n",sock,firstLine);
    //trying to do some error handling, wasn't working very well
    /*if(len==-1){
     printf("error in read line\n");
     return 0;
     }
     else
     return 1;*/
    //printf("%s",firstLine);
}

int processPart(int sock,int parts[], FILE *fp){
    
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
    
    if(strcmp(firstLine,"\n")==0)
        return -1;
    
    
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
        receiveBinary(sock,fp,filesize,part,parts,offset);
    
    return 0;

}

void processGetRequest(int * sockets, char * filename, char * directory) {
    struct timeval tv;
    fd_set sockSet;
    
    FD_ZERO(&sockSet);
    
    char secure_file[256];
    //sprintf(secure_file,".%s",filename);
    char command[1024];
    
    int parts[4];
    parts[0]=parts[1]=parts[2]=parts[3]=0;
    char dir[1024];
    char dir1[1024];
    if(strcmp("/",directory)==0){
        sprintf(dir,"%s.%s","",filename);
        sprintf(dir1,"%s%s","",filename);
    }
    else {
        sprintf(dir,"%s.%s",directory,filename);
        sprintf(dir1,"%s%s",directory,filename);
    }
    
    
    FILE *fp = fopen(dir,"wb");
    int j=0;
    int selRet = 0;
    int openSocket;
    //select statement;
    //int maxSocket = sockets[numServers-1];
    while(1){
        //reset timer each time around
        FD_ZERO(&sockSet);
        for(j=0;j<numServers;j++){
            if(sockets[j]!=-1) {//only add good sockets
                printf("here\n");
                FD_SET(sockets[j],&sockSet);
            }
        }
        //FD_SET(sockets[0],&sockSet);
        //FD_SET(sockets[1],&sockSet);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        selRet = select(maxSocket+1,&sockSet,NULL,NULL,&tv);
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
            for(int i=0;i<numServers;i++){
                printf("check set ");
                printf("%d\n",sockets[i]);
                if (sockets[i]!=-1 && FD_ISSET(sockets[i], &sockSet)) {
                    printf("socket\n");
                    printf("what socket: %d\n",sockets[i]);
                    openSocket = processPart(sockets[i],parts,fp);
                    printf("successread: %d\n",openSocket);
                    if(openSocket==-1)
                        sockets[i]=-1;
                }
            }
        }
    }
    
    if(parts[0]!=1 || parts[1]!=1 || parts[2]!=1 || parts[3]!=1){
        printf("file incomplete\n");
    }
    else {
        sprintf(command,"openssl base64 -d -out %s -in %s -k %s",dir1,dir,password);
        
        system(command);
    }
    

}

int processListServer(int sock,HashFileTable *ht){
    char firstLine[1024];
    char request[8];
    char filename[100];
    char prefix[100];
    char suffix[100];
    long filesize;
    int part;
    long offset;
    char delim[2] = ".";
    char *token;
    
    readList(firstLine,sock);
    printf("readline: %s\n",firstLine);
    
    if(strcmp(firstLine,"\n")==0)
        return -1;
    char* pPosition = strchr(firstLine, '.');
    if(pPosition==NULL){
        File_ f;
        strtok(firstLine,"\n");
        sprintf(filename,"%s/",firstLine);
        strcpy(f.filename,filename);
        FileList * node = find(ht,f);
        if(node==NULL){
            printf("new node: %s\n",f.filename);
            f.part[0]=1;
            f.part[1]=1;
            f.part[2]=1;
            f.part[3]=1;
            insert(ht,f);
        }
        return 0;
        
        
    }
    token = strtok(firstLine,delim);
    //token = strtok(NULL,delim);
    strcpy(prefix,token);
    printf("prefix: %s\n",prefix);
    token= strtok(NULL,delim);
    strcpy(suffix,token);
    printf("suffix: %s\n",suffix);
    token= strtok(NULL,delim);
    part = atoi(token);
    printf("part: %d\n",part);
    
    sprintf(filename,"%s.%s",prefix,suffix);
    
    File_ f;
    strcpy(f.filename,filename);
    //f.part[part-1]=1;
    FileList * node = find(ht,f);
    if(node==NULL){
        printf("new node: %s\n",f.filename);
        f.part[part-1]=1;
        insert(ht,f);
    }
    else
        (node->f).part[part-1]=1;
    
    return 0;
    
}

void processListRequest(int * sockets) {
    struct timeval tv;
    fd_set sockSet;
    
    FD_ZERO(&sockSet);
    
    
    int parts[4];
    
    int j=0;
    int selRet = 0;
    int openSocket;
    
    //select statement;
    HashFileTable *list = createHashTable(100);
    while(1){
        //reset timer each time around
        FD_ZERO(&sockSet);
        for(j=0;j<numServers;j++){
            if(sockets[j]!=-1) {//only add good sockets
                printf("here\n");
                FD_SET(sockets[j],&sockSet);
            }
        }
        //FD_SET(sockets[0],&sockSet);
        //FD_SET(sockets[1],&sockSet);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        selRet = select(maxSocket+1,&sockSet,NULL,NULL,&tv);
        if(selRet==0){
            printf("we timed out\n");
            break;
        }
        
        else if(selRet==-1){
            printf("error in fd set\n");
            break;
        }
        else {//success
            for(int i=0;i<numServers;i++){
                if (sockets[i]!=-1 && FD_ISSET(sockets[i], &sockSet)) {
                    openSocket = processListServer(sockets[i],list);
                    printf("successread: %d\n",openSocket);
                    if(openSocket==-1)
                        sockets[i]=-1;
                }
            }
        }
    }
    //File_ test_file;
    //strcpy(test_file.filename,"test.txt");
    //FileList *test = find(list,test_file);
    //printf("found: %d\n",test!=NULL);
    //printf("part0: %d\n",(test->f).part[0]);
    //printf("part0: %d\n",(test->f).part[1]);
    //printf("part0: %d\n",(test->f).part[2]);
    //printf("part0: %d\n",(test->f).part[3]);
    //iterate through hash and print if complete
    int i;
    FileList *node, *temp;
    
    printf("LIST of Files\n");
    
    if (list!=NULL) {
    
    /* Free the memory for every item in the table, including the
     * strings themselves.
     */
    for(i=0; i<list->size; i++) {
        node = list->table[i];
        while(node!=NULL) {
            temp = node;
            if((node->f).part[0]==1 && (node->f).part[1]==1 &&
               (node->f).part[2]==1 && (node->f).part[3]==1)
                printf("%s\n",(node->f).filename);
            else
                printf("%s [file incomplete]\n",(node->f).filename);
            node = node->next;
            free(temp);
        }
    }
    
    /* Free the table itself */
    free(list->table);
        
    free(list);
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
    //int numServers=2;
    char directory[256];
    char file_dir[1024];
    
    char contentHeader[100];
    long x = 0;
    
    int result = 0;
    
    char buffer[256];
    
    for(int k=0;k<numServers;k++){
        printf("socket number: %d\n",sockets[k]);
    }

    maxSocket=sockets[numServers-1];
    while(1){
        printf("Please enter the message: ");
        bzero(buffer,256);
        fgets(buffer,255,stdin);
        
        for(int k=0;k<numServers;k++){
            printf("socket number: %d\n",sockets[k]);
        }
        
        token=strtok(buffer,delim);
        strcpy(request,token);
        
        if(strcmp("CLOSE\n",request)==0){
            sprintf(contentHeader,"CLOSE %s %s %lu %d %lu\n","test.txt","dumb",x,0,x);
            for(int i=0;i<numServers;i++){
                send(sockets[i],contentHeader,strlen(contentHeader),0);
                close(sockets[i]);
            }
            int result=1;
            break;
            
        }
        
        
        else if(strcmp("GET",request)==0) {
            token=strtok(NULL,delim);
            strcpy(filename,token);
            
            
            token=strtok(NULL,delim);
            
                strcpy(directory,token);
                strtok(directory,"\n");
            
           
            
            
            sprintf(contentHeader,"GET %s %s %lu %d %lu\n",filename,directory,x,0,x);
            int success;
            for(int i=0;i<numServers;i++) {
                success=send(sockets[i],contentHeader,strlen(contentHeader),0);
                printf("success: %d\n",success);
                if(success==-1) {
                    sockets[i]=-1;
                }
            }
            processGetRequest(sockets,filename,directory);
        }
        
        else if(strcmp("LIST",request)==0){
            printf("process list request\n");
            token = strtok(NULL,delim);
            strcpy(filename,token);
            strtok(filename,"\n");
            printf("filename: %s\n",filename);
            sprintf(contentHeader,"LIST %s %s %lu %d %lu\n",filename,"dumb",x,0,x);
            for(int i=0;i<numServers;i++)
                send(sockets[i],contentHeader,strlen(contentHeader),0);
            processListRequest(sockets);
            //printf("to be implemented\n");
        }
        
        
        else if(strcmp("PUT",request)==0){
            token=strtok(NULL,delim);
            strcpy(filename,token);
            
            printf("filename: %s\n",filename);
            token=strtok(NULL,delim);
            
           
                strcpy(directory,token);
                strtok(directory,"\n");
            
            
            //sprintf(file_dir,"%s%s",directory,filename);
            sendFile(sockets,filename,directory);
        }
        
        else if(strcmp("MKDIR",request)==0){
            token=strtok(NULL,delim);
            strcpy(filename,token);
            strtok(filename,"\n");
            sprintf(contentHeader,"MKDIR %s %s %lu %d %lu\n",filename,"dumb",x,0,x);
            for(int i=0;i<numServers;i++)
                send(sockets[i],contentHeader,strlen(contentHeader),0);
        }
        
        
        
        else {
            printf("bad command: shutting down\n");
            printf(contentHeader,"CLOSE %s %s %lu %d %lu\n","test.txt","dumb",x,0,x);
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
    struct sockaddr_in serv_addr2;
    struct sockaddr_in serv_addr3;
    struct hostent *server;
    int ports[4];
    char server_port[64];
    char username[256];
    //char password[256];
    char * token;
    char delim[2]=" ";
    char line[1024];
    
    FILE * config = fopen(argv[1],"r");
    int i=0;
    while(fgets(line,1024,config)!=NULL){
        token=strtok(line,delim);
        
        if(strcmp("Server",token)==0){
            token=strtok(NULL,delim);
            token=strtok(NULL,delim);
            strcpy(server_port,token);
            printf("serverport: %s\n",server_port);
            token=strtok(server_port,":");
            server = gethostbyname(token);
            token = strtok(NULL,"\n");
            printf("token: %s\n",token);
            ports[i]=atoi(token);
            printf("token: %d\n",ports[i]);
            i++;
        }
        else if(strcmp("Username:",token)==0){
            token=strtok(NULL,delim);
            strcpy(username,token);
            token=strtok(username,"\n");
            printf("username: %s\n",username);
        }
        
        else if(strcmp("Password:",token)==0){
            token=strtok(NULL,delim);
            strcpy(password,token);
            token=strtok(password,"\n");
            printf("password: %s\n",password);
            //strcpy(password,token);
        }
        
    }
    

    
    
    
    
    char contentHeader[1024];
    long x = 0;

    
    char buffer[1024];
    
    //int numServers=2;
    
    
    
    int sockets[4];
    
    /*if (argc < 3) {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        exit(0);
    }*/
    
    
    
    /*ports[0] = atoi(argv[2]);
    ports[1]= 10002;
    ports[2] = 10003;
    ports[3] = 10004;*/
    
    /* Create a socket point */
    //sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockets[0] = socket(AF_INET, SOCK_STREAM, 0);
    sockets[1] = socket(AF_INET, SOCK_STREAM, 0);
    sockets[2] = socket(AF_INET, SOCK_STREAM, 0);
    sockets[3] = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sockets[0] < 0) {
        perror("ERROR opening socket");
        exit(1);
    }
    
    
    
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
        printf("ERROR connecting to server %d\n",ports[0]);
        //exit(1);
    }
    
    bzero((char *) &(serv_addr1), sizeof(serv_addr1));
    (serv_addr1).sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&(serv_addr1).sin_addr.s_addr, server->h_length);
    (serv_addr1).sin_port = htons(ports[1]);
    
    /* Now connect to the server */
    if (connect(sockets[1], (struct sockaddr*)&(serv_addr1), sizeof(serv_addr1)) < 0) {
        printf("ERROR connecting to server %d\n",ports[1]);
        //exit(1);
    }
    
    bzero((char *) &(serv_addr2), sizeof(serv_addr2));
    (serv_addr2).sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&(serv_addr2).sin_addr.s_addr, server->h_length);
    (serv_addr2).sin_port = htons(ports[2]);
    
    /* Now connect to the server */
    if (connect(sockets[2], (struct sockaddr*)&(serv_addr2), sizeof(serv_addr2)) < 0) {
        printf("ERROR connecting to server %d\n",ports[2]);
        sockets[2]=-1;
        //exit(1);
    }
    
    bzero((char *) &(serv_addr3), sizeof(serv_addr3));
    (serv_addr3).sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&(serv_addr3).sin_addr.s_addr, server->h_length);
    (serv_addr3).sin_port = htons(ports[3]);
    
    /* Now connect to the server */
    if (connect(sockets[3], (struct sockaddr*)&(serv_addr3), sizeof(serv_addr3)) < 0) {
        printf("ERROR connecting to server %d\n",ports[3]);
        //exit(1);
    }
    
    /* Now ask for a message from the user, this message
     * will be read by server
     */
    
    //Now check passwords
    sprintf(contentHeader,"User %s %s %lu %d %lu\n",username,"dumb",x,0,x);
    for(int i=0;i<numServers;i++)
        send(sockets[i],contentHeader,strlen(contentHeader),0);
    
    //strcpy(password,"pwd");
    
    printf("we want to send the password\n");
    sprintf(contentHeader,"Password %s %s %lu %d %lu\n",password,"dumb",x,0,x);
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