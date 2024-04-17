#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/ipc.h>

#define N 6 // Numero binari
#define T 5 // Millisecondi di permanenza
#define Tmin 3 // Millisecondi minimi prima del prossimo treno
#define Tmax 7 // Millisecondi massimi prima del prossimo treno

int attesa = 0; // Treni in attesa di passare sul binario
int passato = 0; // Treni che hanno attraversato
int presente = 0; // Treni presenti sui binari

static pthread_mutex_t mutex_id = PTHREAD_MUTEX_INITIALIZER; // Mutex per assegnazione ID
static pthread_mutex_t mutex_attesa = PTHREAD_MUTEX_INITIALIZER; // Mutex per stampa treni in attesa
static pthread_mutex_t mutex_attraversano = PTHREAD_MUTEX_INITIALIZER; // Mutex per stampa treni che attraversano

int id = 0; // ID treni

typedef struct ListaTreni{
    int id;
    ListaTreni* next;
    ListaTreni* before;
} ListaTreni;

void createList(ListaTreni* lista){
    lista = malloc(sizeof(struct ListaTreni));
    lista->id = 0;
    lista->before = NULL;
    lista->next = NULL;
}

void insertID(ListaTreni* lista, int id){
    if(lista == NULL){
        perror("Lista non inizializzata");
        exit(1);
    }
    lista->id = id;
}

ListaTreni insertLista(ListaTreni* lista, int id){
    ListaTreni l;
    createList(&l);
    insertID(&l, id);
    l.next = &lista;
    return l;
}

void transit(int binari){
    int ret;
    ListaTreni* l;
    createList(l);

    pthread_mutex_lock(&mutex_id);
    insertID(l, id);
    id++;
    lista = insertLista(l, id);
    pthread_mutex_unlock(&mutex_id);

    pthread_mutex_lock(&mutex_attesa);
    attesa++;
    pthread_mutex_unlock(&mutex_attesa);

    for(int i = 0; i < N; i++){
        pthread_mutex_lock(&mutex_attraversano);
        attesa--;
        struct sembuf sem_op = {i, -1, 0};

        ret = semop(binari, &sem_op, 1);
        if(ret == -1){
            perror("Errore semop");
            exit(1);
        }

        presente++;

        sem_op.sem_op = 1;
        ret = semop(binari, &sem_op, 1);
        if(ret == -1){
            perror("Errore semop");
            exit(1);
        }

        presente--;
        passato++;        
        pthread_mutex_unlock(&mutex_attraversano);
    }
}

ListaTreni lista;

int main(){
    int ret;

    createList(&lista);

    // Creazione set semafori
    int binari = semget(IPC_PRIVATE, N, IPC_CREAT | IPC_EXCL | 0600);
    if(binari == -1){
        perror("Errore semget");
        exit(1);
    }

    // Struct per invio comandi al semaforo
    union semun{
        int val;
        struct semid_ds* buf;
        unsigned short* array;
        struct seminfo* __buf;
    };

    // Inizializzazione set semafori a 0;
    union semun sem_val = {.val = 0};
    for(int i = 0; i < N; i++){
        ret = semctl(binari, i, SETVAL, sem_val);
        if(ret == -1){
            perror("Errore semctl");
            printf("\nErrore semaforo %d\n", i);
            exit(1);
        }
    }

    ret = pthread_mutex_init(&mutex_id, NULL);
    if(ret == -1){
        perror("Errore init mutex_id");
        exit(1);
    }

    ret = pthread_mutex_init(&mutex_attesa, NULL);
    if(ret == -1){
        perror("Errore init mutex_attesa");
        exit(1);
    }

    ret = pthread_mutex_init(&mutex_attraversano, NULL);
    if(ret == -1){
        perror("Errore init mutex_attraversano");
        exit(1);
    }

    while(1){
        pthread_t thread;

        ret = pthread_create(&thread, NULL, transit, binari);
        if(ret != 0){
            perror("Errore pthread_create");
            printf("\nErrore create thread %d\n", id);
            exit(1);
        }

        ret = pthread_detach(thread);
        if(ret != 0){
            perror("Errore pthread_detach");
            printf("\nErrore detach thread %d\n", id);
            exit(1);
        }
    }

    return 0;
}