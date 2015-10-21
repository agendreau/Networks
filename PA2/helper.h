
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


//http://www.sparknotes.com/cs/searching/hashtables/section3/page/2/

typedef struct {
    char username[256];
    char password[256];
    int usernumber;
} User;

struct Node {
    User u;
    struct Node *next;
};

struct Node;
typedef struct Node Node;

typedef struct {
    int size;
    Node **table;
} HashTable;


HashTable *createHashTable(int size)
{
    HashTable *newTable;
    
    if (size<1) return NULL; /* invalid size for table */
    
    /* Attempt to allocate memory for the table structure */
    if ((newTable = malloc(sizeof(unsigned int))) == NULL) {
        return NULL;
    }
    
    /* Attempt to allocate memory for the table itself */
    if ((newTable->table = malloc(sizeof(Node *) * size)) == NULL) {
        return NULL;
    }
    
    /* Initialize the elements of the table */
    for(int i=0; i<size; i++) newTable->table[i] = NULL;
    
    /* Set the table's size */
    newTable->size = size;
    
    return newTable;
}

unsigned int hashValue(User u,HashTable *ht){
    //unsigned int hv = strlen(u.username)+strlen(u.password) % (ht->size);
    unsigned int hv = strlen(u.username) % (ht->size);
    //printf("hash value: %u\n",hv);
    return hv;
}

Node * findUserNames(HashTable *ht,User u){
    unsigned int hv = hashValue(u,ht);
    Node *node;
    for(node=ht->table[hv];node!=NULL;node=node->next){
        if(strcmp(u.username,(node->u).username)==0) //&& strcmp(u.password,(node->u).password)==0)
            return node;
    }
    return NULL;
    
}

Node * find(HashTable *ht,User u){
    unsigned int hv = hashValue(u,ht);
    Node *node;
    for(node=ht->table[hv];node!=NULL;node=node->next){
        if(strcmp(u.username,(node->u).username)==0 &&
           strcmp(u.password,(node->u).password)==0)
            return node;
    }
    return NULL;
    
}

int insert(HashTable *ht,User u){
    Node *newNode;
    Node *currentNode;
    Node *temp1;
    Node *temp2;
    unsigned int hv = hashValue(u,ht);
    
    if ((newNode = malloc(sizeof(Node))) == NULL) return 1; //failed to allocate memory
    if ((temp1 = malloc(sizeof(Node))) == NULL) return 1;
    if ((temp2 = malloc(sizeof(Node))) == NULL) return 1;
    
    currentNode = find(ht,u);
    if(currentNode!=NULL) return 2; //already exists
    currentNode = findUserNames(ht,u);
    temp1 = currentNode;
    int number=0;
    while(temp1!=NULL) {
        if(strcmp((temp1->u).username,u.username)==0)
            number+=1;
        temp2 = temp1;
        temp1 = temp2->next;
    }
    printf("new number: %d\n",number);
    u.usernumber=number;
    newNode->u = u;
    newNode->next = ht->table[hv];
    printf("node number: %d\n",(newNode->u).usernumber);
    ht->table[hv]=newNode;
    free(temp1);
    free(temp2);
    //printf("Name: %s\n",(ht->table[hv]->u).username);
    //printf("Password: %s\n",(ht->table[hv]->u).password);
    return 0;
}

void free_table(HashTable *ht)
{
    int i;
    Node *node, *temp;
    
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


