#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <errno.h>

#define N 10 // Numero binari
#define T 2 // Millisecondi di permanenza
#define Tmin 5 // Millisecondi minimi prima del prossimo treno
#define Tmax 19 // Millisecondi massimi prima del prossimo treno

int attesa = 0; // Treni in attesa di passare sul binario
int passato = 0; // Treni che hanno attraversato
int presente = 0; // Treni presenti sui binari

static pthread_mutex_t mutex_id = PTHREAD_MUTEX_INITIALIZER; // Mutex per assegnazione ID
static pthread_mutex_t mutex_attesa = PTHREAD_MUTEX_INITIALIZER; // Mutex per stampa treni in attesa
static pthread_mutex_t mutex_attraversano = PTHREAD_MUTEX_INITIALIZER; // Mutex per stampa treni che attraversano

int id = 0; // ID treni

int msleep(long msec){
    struct timespec ts;
    int res;

    if (msec < 0){
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do{
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

void transit(void* b){
    int ret;
    int binari = *(int*)b;
    int t = rand() % (Tmax - Tmin + 1) + Tmin;

    pthread_mutex_lock(&mutex_id);
    id++;
    pthread_mutex_unlock(&mutex_id);

    pthread_mutex_lock(&mutex_attesa);
    attesa++;
    printf("\nAttesa: %d\nPresenti: %d\nPassati: %d\n", attesa, presente, passato);
    pthread_mutex_unlock(&mutex_attesa);

    for(int i = 0; i < N; i++){
        msleep(t);
        struct sembuf sem_op = {i, -1, 0};

        ret = semop(binari, &sem_op, 1);
        if(ret == -1){
            perror("Errore semop");
            exit(1);
        }

        attesa--;

        pthread_mutex_lock(&mutex_attraversano);
        presente++;
        printf("\nAttesa: %d\nPresenti: %d\nPassati: %d\n", attesa, presente, passato);
        msleep(T);
        pthread_mutex_unlock(&mutex_attraversano);

        presente--;
        passato++;
        printf("\nAttesa: %d\nPresenti: %d\nPassati: %d\n", attesa, presente, passato);

        sem_op.sem_op = 1;
        ret = semop(binari, &sem_op, 1);
        if(ret == -1){
            perror("Errore semop");
            exit(1);
        }
    }

    pthread_exit(0);
}

int main(){
    srand(time(NULL));
    int ret;

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

    // Inizializzazione set semafori a N;
    union semun sem_val = {.val = N};
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

        ret = pthread_create(&thread, NULL, (void*) transit, &binari);
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