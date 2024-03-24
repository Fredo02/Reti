#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>

#define PORT 5000

// Struttura dati del Pacchetto che contiene l'id del nodo che lo manda (int), il proprio valore (int) e il proprio numero di sequenza (int)
typedef struct Packet{
    int node_id;
    int value;
    int sequence_number;
} Packet;

void log(int id, char * format, ...) {
    va_list args;
    va_start(args, format);
    printf("%d: ", id);
    printf(format, args);
    va_end(args);
}

// Funzione per l'invio di un pacchetto in broadcast
void send_broadcast(int sockfd, Packet packet) {
    struct sockaddr_in broadcastaddr;
    socklen_t broadcastaddr_len;

    // Impostazione dell'indirizzo broadcast
    broadcastaddr.sin_family = AF_INET;
    broadcastaddr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcastaddr.sin_port = htons(PORT);

    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    char id[256];
    char value[256];
    char sequence_number[256];

    sprintf(id, "%d", packet.node_id);
    id[strlen(id)] = '\0';
    sprintf(value, "%d", packet.value);
    value[strlen(value)] = '\0';
    sprintf(sequence_number, "%d", packet.sequence_number);
    sequence_number[strlen(sequence_number)] = '\0';

    strcat(buffer, id);
    strcat(buffer, ":");
    strcat(buffer, value);
    strcat(buffer, ":");
    strcat(buffer, sequence_number);
    buffer[strlen(buffer)] = '\0';

    // Invio del pacchetto
    broadcastaddr_len = sizeof(broadcastaddr);
    sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&broadcastaddr, broadcastaddr_len);
}

// Funzione per la ricezione di un pacchetto
Packet receive_packet(int sockfd) {
    Packet packet;
    struct sockaddr_in clientaddr;
    socklen_t clientaddr_len;

    char buffer[256];
    int id, value, sequence_number;

    // Ricezione del pacchetto
    clientaddr_len = sizeof(clientaddr);
    recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&clientaddr, &clientaddr_len);
    buffer[strlen(buffer) + 1] = '\0';

    sscanf(buffer, "%d:%d:%d", &id, &value, &sequence_number);

    packet.node_id = id;
    packet.value = value;
    packet.sequence_number = sequence_number;

    return packet;
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 3) {
        perror("No neighbor for this node");
        return 1;
    }

    int id = atoi(argv[1]);

    int * neighbors = calloc(sizeof(int), argc - 2);
    for (int i = 0; i < argc - 2; i ++) {
        neighbors[i] = atoi(argv[i + 2]);
    }

    log(id, "begin\n");

    // Implementation
    srand(time(NULL));
    int sockfd, sequence_number, ret, new_sockfd;
    struct sockaddr_in srv_addr, new_srv_addr;
    Packet packet;

    sockfd = socket (AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0){
        perror("Errore socket sockfd");
        exit(1);
    }

    int broadcastPermission = 1;
    ret = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission, sizeof(broadcastPermission));
    if(ret < 0){
        perror("Errore setsockopt sockfd");
        exit(1);
    }

    memset((char *)&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    srv_addr.sin_port = htons(PORT);

    //open read socket
    new_sockfd = socket (AF_INET, SOCK_DGRAM, 0);
    if(new_sockfd < 0){
        perror("Errore new_sockfd");
        exit(1);
    }

    int reuseAddressPermission = 1;
    ret = setsockopt(new_sockfd, SOL_SOCKET, SO_REUSEADDR, (void *) &reuseAddressPermission, sizeof(reuseAddressPermission));
    if(ret < 0){
        perror("Errore setsockopt new_sockfd");
        exit(1);
    }

    memset((char*)&new_srv_addr, 0, sizeof(new_srv_addr));
    new_srv_addr.sin_family = AF_INET;
    new_srv_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    new_srv_addr.sin_port = htons(PORT);   
    
    ret = bind(new_sockfd, (struct sockaddr *)&new_srv_addr, sizeof(new_srv_addr));
    if(ret < 0){
        perror("Errore bind new_sockfd");
        exit(1);
    }

    sequence_number = 0;

    if(id == 0){
        sleep(1);
        packet.node_id = id;
        packet.sequence_number = sequence_number++;
        packet.value = rand();
        send_broadcast(sockfd, packet);
    }

    int j = 0;
    while (1) {
        sleep(10);
        packet = receive_packet(new_sockfd);

        if(packet.node_id == id) continue;

        for (int i = 0; i < argc; i ++) {
            if (packet.sequence_number <= sequence_number || packet.node_id == neighbors[i]) {
                continue;
            }
        }

        printf("Nodo %d: ricevuto messaggio da nodo %d con valore %d (sequenza %d)\n",
               id, packet.node_id, packet.value, packet.sequence_number);

        sequence_number++;

        if(j < argc){
            packet.node_id = neighbors[j];
            packet.sequence_number = sequence_number;
        }
        else break;
        j++;
        send_broadcast(sockfd, packet);
    }

    free(neighbors);
    ret = close(sockfd);
    if(ret < 0){
        perror("Errore close sockfd");
        exit(1);
    }
    ret = close(new_sockfd);
    if(ret < 0){
        perror("Errore close new_sockfd");
        exit(1);
    }

    return 0;
}