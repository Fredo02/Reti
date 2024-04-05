#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <semaphore.h>

#define N 6 // Numero binari
#define T 5 // Millisecondi di permanenza
#define Tmin 3 // Millisecondi minimi prima del prossimo treno
#define Tmax 7 // Millisecondi massimi prima del prossimo treno

int attesa; // Treni in attesa di passare sul binario
int passato; // Treni che hanno attraversato
int presente; // Treni presenti sui binari

sem_t id_treno;
sem_t treni;
sem_t lista;

typedef struct ListaTreni{
    int id;
    ListaTreni* next;
    ListaTreni* before;
} ListaTreni;

void createList(ListaTreni* lista, int id){
    lista = malloc(sizeof(struct ListaTreni));
    lista->id = id;
    lista->before = NULL;
    lista->next = NULL;
}

ListaTreni insertId(ListaTreni* lista, int id){
    ListaTreni l;
    createList(&l, id);
    l.next = lista;
    return l;
}

void transit(int id){

}

int main(){
    int ret;

    ret = sem_init(&id_treno, 0, 1);
    if(ret == -1){
        perror("Errore sem_init id_treno");
        exit(1);
    }

    ret = sem_init(&treni, 0, N);
    if(ret == -1){
        perror("Errore sem_init treni");
        exit(1);
    }

    ret = sem_init(&lista, 0, 1);
    if(ret == -1){
        perror("Errore sem_init lista");
        exit(1);
    }

    ret = sem_init(&id_treno, 0, 1);
    if(ret == -1){
        perror("Errore sem_init id_treno");
        exit(1);
    }

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

    // Inizializzazione semafori a 0;
    union semun sem_val = {.val = 0};
    for(int i = 0; i < N; i++){
        ret = semctl(binari, i, SETVAL, sem_val);
        if(ret == -1){
            perror("Errore semctl");
            printf("\nErrore semaforo %d\n", i);
            exit(1);
        }
    }

    int id = 0; // ID treni
    while(1){
        pthread_t thread;

        ret = pthread_create(&thread, NULL, transit, id);
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

        id++; // Incremento id treno
    }

    return 0;
}