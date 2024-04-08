#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#define PORT 7379

sem_t sem_sock;

struct Argouments{
    int sockfd;
    struct LinkedList* ll;
};

struct Node{
    char* key;
    char* value;
    int time_entry;
    int time_tot;
    struct Node* next;
};

struct LinkedList{
    struct Node* head;
};

void exit_with_error(const char * msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void removeElement(struct LinkedList* ll, char* key){
    if(ll->head == NULL) return;

    // Chiave da rimuovere sulla testa
    if(strcmp(ll->head->key, key) == 0){
        struct Node* node = ll->head;
        ll->head = ll->head->next;
        free(node);
        return;
    }

    // Trovare il nodo dove si trova la chiave
    struct Node* current_node = ll->head;
    struct Node* prev_node = NULL;
    while(current_node != NULL && strcmp(current_node->key, key) != 0){
        prev_node = current_node;
        current_node = current_node->next;
    }

    // Chiave non trovata
    if(current_node == NULL){
        return;
    }

    // Rimozione nodo
    prev_node->next = current_node->next;
    free(current_node);
}

void add(struct LinkedList* ll, char* key, char *value, int time_entry, int time_tot){
    struct Node* node = (struct Node*)malloc(sizeof(struct Node));
    node->key = key;
    node->value = value;
    node->time_entry = time_entry;
    node->time_tot = time_tot;
    node->next = NULL;

    if(ll->head == NULL){
        ll->head = node;
    }
    else{
        struct Node* current_node = ll->head;
        while(current_node->next != NULL){
            current_node = current_node->next;
        }
        current_node->next = node;
    }
}

void freeLinkedList(struct LinkedList* ll){
    struct Node* current_node = ll->head;
    while (current_node != NULL) {
        struct Node* node = current_node;
        current_node = current_node->next;
        free(node);
    }
    ll->head = NULL;
}

void print(struct LinkedList* ll){
    struct Node* current_node = ll->head;

    while(current_node != NULL){
        printf("Key: %s, Value: %s, Enter time: %d, total time: %d\n ", current_node->key, current_node->value, current_node->time_entry, current_node->time_tot);
        current_node = current_node->next;
    }
}

struct Node* search(struct LinkedList* ll, char* key){
    struct Node* current_node = ll->head;

    while(current_node != NULL){
        if(strcmp(current_node->key, key) == 0){
            return current_node;
        }
        current_node = current_node->next;
    }
    return NULL;
}

void reading_from_socket(struct Argouments* arg){
    char buf[1024];

    while(1){
        memset(buf, '\0', sizeof(buf));

        int bytes_read = recv(arg->sockfd, buf, sizeof(buf), 0);  
        if(bytes_read == 0) break;

        char* cmd = strtok(buf, "\r\n");

        while(cmd != NULL){            
            // SET
            if(strcmp(cmd, "SET") == 0){
                char* value= calloc(64,sizeof(char));
                char* key = calloc(64,sizeof(char));
                int time_entry = -1;
                int time_tot = -1;

                for(int i = 0; i < 8; i++){
                    cmd = strtok(NULL,"\r\n");
                    if(cmd == NULL) break;

                    if(i == 1){
                        strcpy (key, cmd);
                    }
                    else if(i == 3){
                        strcpy(value, cmd);
                    }
                    else if(i == 7){
                        time_tot = atoi(cmd);
                        time_entry = time(NULL);
                    } 
                }

                if(sem_wait(&sem_sock) == -1) exit_with_error("Errore sem_wait SET"); 
                add(arg->ll, key, value, time_entry, time_tot);
                if(sem_post(&sem_sock) == -1) exit_with_error("Errore sem_post SET");

                int bytes_written = write(arg->sockfd, "+OK\r\n", strlen("+OK\r\n"));
                if(bytes_written == -1) exit_with_error("Errore write SET");
            }

            if(cmd == NULL) break;

            // GET
            if(strcmp(cmd, "GET") == 0){
                char* key = calloc(64,sizeof(char));
                for(int i = 0; i < 2; i++){
                    cmd = strtok(NULL,"\r\n");
                    if(i == 1){
                       strcpy (key, cmd);
                    }
                }

                if(sem_wait(&sem_sock) == -1) exit_with_error("Errore sem_wait GET"); 

                struct Node* node = search(arg->ll, key);

                if(node == NULL){
                    int bytes_written = write(arg->sockfd, "$-1\r\n", strlen("$-1\r\n"));
                    if(bytes_written == -1) exit_with_error("Errore write GET");
                }
                else{
                    int actual_time = time(NULL);
                    if(node->time_entry != -1 && node->time_tot != -1 && actual_time - node->time_entry > node->time_tot){
                        removeElement(arg->ll, key);
                        int bytes_written = write(arg->sockfd, "$-1\r\n", strlen("$-1\r\n"));
                        if (bytes_written == -1) exit_with_error("Errore write GET");
                    }
                    else{
                        char response[256];
                        sprintf(response,"$%ld\r\n%s\r\n", strlen(node->value), node->value);

                        int bytes_written = write(arg->sockfd, response, strlen(response));
                        if(bytes_written == -1) exit_with_error("Errore write GET");
                    }
                }
                if(sem_post(&sem_sock) == -1) exit_with_error("Errore sem_post GET"); 
                free(key);    
            }

            // CLIENT
            if(strcmp(cmd, "CLIENT") == 0){
                int bytes_written = write(arg->sockfd, "+OK\r\n", strlen("+OK\r\n"));
                if(bytes_written == -1) exit_with_error("Errore write CLIENT");
                break;
            }

            cmd = strtok(NULL,"\r\n");
        }  
    }

    free(arg);
}

// https://redis.io/docs/reference/protocol-spec/

int main(int argc, const char * argv[]) {
    struct LinkedList* ll = malloc(sizeof(struct LinkedList));
    ll->head = NULL;

    // Open Socket and receive connections

    // Keep a key, value store (you are free to use any data structure you want)

    // Create a process for each connection to serve set and get requested

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) exit_with_error("Errore creazione socket");

    int en = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(int));

    struct sockaddr_in server_addr;
    int port_num = PORT;
    memset((char*)&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port_num);

    if(bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) exit_with_error("Errore bind socket");

    if(listen(sockfd, 10) == -1) exit_with_error("Errore listen");

    if(sem_init(&sem_sock, 1, 1) == -1) exit_with_error("Errore sem_init");

    while(1){
        pthread_t thread;
        
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if(client_sockfd == -1){
            exit_with_error("Errore accept");
        }

        struct Argouments* arg = malloc(sizeof(struct Argouments));
        arg->ll = ll;
        arg->sockfd = client_sockfd;

        pthread_create(&thread, NULL, (void *)reading_from_socket, (void*)arg);       
    }

    close(sockfd);
    freeLinkedList(ll);
    free(ll);
}