#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>


#define BUCKETS 256
char sentinel = {'\0'};//null terminated string for the sentinal

//ALL THE FUNCTIONS FOR THE QUEUE, HASH, and LIST ARE FROM THE TEXTBOOK

// defining the data structures
typedef struct node_t {
    char *value;
    int count;
    struct node_t *next;
} node_t;

typedef struct list_t {
    node_t *head;
    pthread_mutex_t lock;
} list_t;

typedef struct hash_t {
    list_t lists[BUCKETS];
} hash_t;

typedef struct queue_t {
    node_t *head;
    node_t *tail;
    pthread_mutex_t headLock, tailLock;
    pthread_cond_t notEmpty;
} queue_t;

// functions for initializing
void List_Init(list_t *L) {
    L->head = NULL;
    pthread_mutex_init(&L->lock, NULL);
}

void Hash_Init(hash_t *H) {
    int i;
    for (i = 0; i < BUCKETS; i++) {
        List_Init(&H->lists[i]);
    }
}

void initQueue(queue_t *q) {
    node_t *tmp = malloc(sizeof(node_t));
    tmp->next = NULL;
    q->head = q->tail = tmp;
    pthread_mutex_init(&q->headLock, NULL);
    pthread_mutex_init(&q->tailLock, NULL);
    pthread_cond_init(&q->notEmpty, NULL);
}

int List_Insert(list_t *L, char *key) {
    node_t *new = malloc(sizeof(node_t));
    if (new == NULL) {
        perror("malloc");
        free(new);
        return -1;
    }
    new->value = strdup(key);
    new->next = L->head;
    L->head = new;
    return 0;
}

int List_Lookup(list_t *L, char *key) {
    int rv = -1;
    node_t *curr = L->head;
    while (curr) {
        if (strcmp(curr->value, key) == 0) {
            rv = 0;
            break;
        }
        curr = curr->next;
    }
    return rv;
}

int hash(const char *s) {
    unsigned short h = 0;
    for (;*s; s++) h += (h >> 4) + *s + (*s << 9);
    return h;
}

int Hash_Insert(hash_t *H, char *key) {
    return List_Insert(&H->lists[hash(key) % BUCKETS], key);
}

int Hash_Lookup(hash_t *H, char *key) {
    return List_Lookup(&H->lists[hash(key) % BUCKETS], key);
}
// functions for queue
void enQueue(queue_t *q, char *value) {
    node_t *tmp = malloc(sizeof(node_t));
    assert(tmp != NULL);
    tmp->value = strdup(value);
    tmp->next = NULL;

    pthread_mutex_lock(&q->tailLock);
    q->tail->next = tmp;
    q->tail = tmp;
    pthread_cond_signal(&q->notEmpty);
    pthread_mutex_unlock(&q->tailLock);
}

int deQueue(queue_t *q, char **value) {
    pthread_mutex_lock(&q->headLock);
    if (q->head->next == NULL) {
        pthread_cond_wait(&q->notEmpty, &q->headLock);//changed returning to the pthread cond wait
    }
    node_t *tmp = q->head;
    node_t *newHead = tmp->next;
    *value = newHead->value;
    q->head = newHead;
    pthread_mutex_unlock(&q->headLock);
    free(tmp);
    return 0;
}

// reader and counter functions
queue_t evenQueue;//global queues
queue_t oddQueue;
hash_t oddHash;//global hash tables
hash_t evenHash;

void *reader(void* filename) {//reader function
    FILE *open = fopen(filename, "r");//opens the file name
    if (open == NULL) {//if the file doesnt exist perror
        perror(filename);
        return NULL;
    }
    char *ptr;
    while (fscanf(open, "%ms", &ptr) != EOF) {//goes until it reaches the end of the file
        int checkBit = ptr[0] & 1;//check the most bottom bit, if it is zero then we make it even
        if(checkBit == 0){
            enQueue(&evenQueue, ptr);//enqueues the current word
        }
        else if(checkBit == 1){
            enQueue(&oddQueue, ptr);//enqueues the current word
        }
        free(ptr);
    }
    fclose(open);
    return NULL;
}

void *counterEven(void *args) {//counter function for even
    int Count = 0;
    Hash_Init(&evenHash);//initiate the evenHash table
    char *word;
    while (deQueue(&evenQueue, &word) == 0) {//dequeues
        if (word[0] == '\0') {//checks for the sentinel value
            free(word);//frees the word
            break;
        }
        if (Hash_Lookup(&evenHash, word) == 0){//checks to see if it is in the hash table
            int index = hash(word) % BUCKETS;//sets the index
            for (node_t *currentEven = evenHash.lists[index].head; currentEven; currentEven = currentEven->next) {//for loop to go through the list
                if (strcmp(currentEven->value, word) == 0) {//checks to see if the node matches the word as there are multiple
                                                            //words in the same hash index
                    currentEven->count++;//increments the count
                    if (currentEven->count > Count) {//updates the count to set the max count
                        Count = currentEven->count;
                    }
                    break;
                }
            }
        } else {
            int index = hash(word) % BUCKETS;
            if (Hash_Insert(&evenHash, word) == 0) {//inserts into the hash table
                for (node_t *currentEven = evenHash.lists[index].head; currentEven; currentEven = currentEven->next) {
                    if (strcmp(currentEven->value, word) == 0) {//checks to see if the word matches the current index
                        currentEven->count = 1;//sets the count
                        if (currentEven->count > Count) {//updates the count
                            Count = currentEven->count;
                        }
                        break;
                    }
                }
            }
        }

    }
    return NULL;
}

//COPY OF COUNTER EVEN
void *counterOdd(void *args) {
    int Count = 0;
    Hash_Init(&oddHash);
    char *word;
    while (deQueue(&oddQueue, &word) == 0) {
        if (word[0] == '\0') {//checks for the sentinel value
            free(word);//frees the word
            break;
        }
        if (Hash_Lookup(&oddHash, word) == 0) {
            int index = hash(word) % BUCKETS;
            for (node_t *currentOdd = oddHash.lists[index].head; currentOdd; currentOdd = currentOdd->next) {
                if (strcmp(currentOdd->value, word) == 0) {
                    currentOdd->count++;
                    if (currentOdd->count > Count) {
                        Count = currentOdd->count;
                    }
                    break;
                }
            }
        } else {
            int index = hash(word) % BUCKETS;
            if (Hash_Insert(&oddHash, word) == 0) {
                for (node_t *currentOdd = oddHash.lists[index].head; currentOdd; currentOdd = currentOdd->next) {
                    if (strcmp(currentOdd->value, word) == 0) {
                        currentOdd->count = 1;
                    }
                    break;
                }
            }
        }
    }
    return NULL;
}


int main(int argc, char **argv) {
    int tempArgc = argc - 1;//gets the amount of files
    initQueue(&oddQueue);//create the oddqueue
    initQueue(&evenQueue);//creates the evenqueue
    pthread_t counterThread[2];//two counter threads
    pthread_t readerThread[tempArgc];//reader threads depending on how many files there are
    for(int i = 1; i < argc; i++){//sets the arguments for the readers
        pthread_create(&readerThread[i-1], NULL, reader, argv[i]);//creates the thread for the queue
    }
    pthread_create(&counterThread[0], NULL, counterEven, NULL);//creates the counter threads
    pthread_create(&counterThread[1], NULL, counterOdd, NULL);
    for (int i = 0; i < tempArgc; i++) {
        pthread_join(readerThread[i], NULL);//joins the queue threads
    }
    enQueue(&evenQueue, &sentinel);//adds in a sentinel value for the end of the queue
    enQueue(&oddQueue, &sentinel);//adds in a sentinel value for the end of the queue
    for (int i = 0; i < 2; i++) {
        pthread_join(counterThread[i], NULL);//joins the queue threads
    }

    int CountOdd = 0;//for setting the maxCount of the odd count
    for (int i = 0; i < BUCKETS; i++) {
        node_t *currentOdd = oddHash.lists[i].head;//creates a node pointer for the oddhash table
        while (currentOdd != NULL) {//checks to see if the head is pointing to something
            if (currentOdd->count > CountOdd) {//checks to see if the count
                CountOdd = currentOdd->count;//sets the max Count
            }
            currentOdd = currentOdd->next;//moves onto the next
        }
    }
    //Repeat the same for the Even count
    int CountEven = 0;
    for (int i = 0; i < BUCKETS; i++) {
        node_t *currentEven = evenHash.lists[i].head;
        while (currentEven != NULL) {
            if (currentEven->count > CountEven) {
                CountEven = currentEven->count;
            }
            currentEven = currentEven->next;
        }
    }

    if(CountEven > CountOdd){//checks to see if the maxCountEven is greater than odd
        for (int i = 0; i < BUCKETS; i++) {
            node_t *currentEven = evenHash.lists[i].head;//hash table pointer
            while (currentEven != NULL) {//checks if it not null
                if (currentEven->count == CountEven) {//checks if the count is equal to the max count
                    printf("%s %d\n", currentEven->value, currentEven->count);//prints out
                }
                currentEven = currentEven->next;
            }
        }
    }
    //Same copy but for the maxcount odd
    if(CountOdd > CountEven){
        for (int i = 0; i < BUCKETS; i++) {
            node_t *currentOdd = oddHash.lists[i].head;
            while (currentOdd != NULL) {
                if (currentOdd->count == CountOdd) {
                    printf("%s %d\n", currentOdd->value, currentOdd->count);
                }
                currentOdd = currentOdd->next;
            }
        }
    }
    return 0;
}