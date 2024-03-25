#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <string.h>

#define PORT 5000

typedef struct Message {
    int source;
    int sequence;
    int payload_lenght;
    char * payload;
} Message;

typedef struct Payload {
    int value;
} Payload;

typedef struct MessageList{
    MessageList* left;
    Message* msg;
    MessageList* right;
} MessageList;

Message * prepare_message(Payload * payload);

//---------------
// Traffic Generator
//---------------

// Some sistem that triggers handler callback every x milliseconds

//---------------
// Broadcaster
//---------------

typedef void (*BroadcasterHandler)(void * broadcaster, Payload * payload);

typedef struct Broadcaster {
    // .. Local data to manage broadcast
    // e.g. socket opened, network interfaces, 
    // sequence of other nodes...
    int sock_fd;
    struct sockaddr_in srv_addr;
    socklen_t srv_len;
    int sequence;
    int id;
    MessageList* list;

    BroadcasterHandler handler;
} Broadcaster;

void register_handler(Broadcaster * broadcaster, BroadcasterHandler handler) {
    broadcaster->handler = handler;
}

void create_send_socket(void *broadcaster, Payload *payload) {
    // Cast the broadcaster pointer
    Broadcaster *b = (Broadcaster *)broadcaster;
    int ret;
    struct sockaddr_in srv_addr;

    // Create the socket
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_fd < 0){
        perror("Errore send socket");
        exit(1);
    }

    // Set SO_BROADCAST option for broadcasting
    int broadcast = 1;
    ret = setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    if(ret < 0){
        perror("Errore setsockopt send socket");
        exit(1);
    }

    // Setup server
    memset((char *)&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    srv_addr.sin_port = htons(PORT);

    // Store the socket in the Broadcaster structure
    b->sock_fd = sock_fd;
    b->srv_addr = srv_addr;
    b->srv_len = sizeof(srv_addr);
}

void create_and_bind_recv_socket(void *broadcaster, Payload *payload) {
    // Cast the broadcaster pointer
    Broadcaster *b = (Broadcaster *)broadcaster;
    int ret;
    struct sockaddr_in srv_addr;

    // Create the socket
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_fd < 0){
        perror("Errore recv socket");
        exit(1);
    }

    // Set SO_REUSEADDR option for broadcasting
    int reuseAddressPermission = 1;
    ret = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (void *) &reuseAddressPermission, sizeof(reuseAddressPermission));
    if(ret < 0){
        perror("Errore setsockopt write socket");
        exit(1);
    }

    // Setup server
    memset((char*)&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    srv_addr.sin_port = htons(PORT);

    // Bind socket to all interfaces using the provided function
    bind_to_all_interfaces(sock_fd, b, NULL);

    // Store the socket in the Broadcaster structure
    b->sock_fd = sock_fd;
    b->srv_addr = srv_addr;
    b->srv_len = sizeof(srv_addr);
}

Message * prepare_message(Payload * payload){
    Message* message = malloc(sizeof(struct Message));
    if(message == NULL){
        perror("Errore malloc message");
        exit(1);
    }

    char* msg = malloc(sizeof(payload->value));
    sscanf(payload->value, "%s", msg);
    msg[strlen(msg)] = '\0';

    message->payload_lenght = strlen(msg);
    memcpy(message->payload, msg, message->payload_lenght);
    message->payload[message->payload_lenght] = '\0';

    return message;
}

void send_udp(Broadcaster * broadcaster, Payload * payload) {
    // Send UDP packet and update local sequence
    Message* msg = malloc(sizeof(struct Message));
    if(msg == NULL){
        perror("Errore malloc msg send");
        exit(1);
    }

    struct sockaddr_in broadcastaddr = broadcaster->srv_addr;
    socklen_t broadcastaddr_len = broadcaster->srv_len;

    msg = prepare_message(payload);
    msg->sequence = broadcaster->sequence;
    msg->source = broadcaster->id;

    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    char id[256];
    char value[256];
    char len[256];
    char sequence_number[256];

    sprintf(id, "%d", msg->source);
    id[strlen(id)] = '\0';
    sprintf(value, "%d", msg->payload);
    value[strlen(value)] = '\0';
    sprintf(len, "%d", msg->payload_lenght);
    len[strlen(len)] = '\0';
    sprintf(sequence_number, "%d", msg->sequence);
    sequence_number[strlen(sequence_number)] = '\0';

    strcat(buffer, id);
    strcat(buffer, ":");
    strcat(buffer, value);
    strcat(buffer, ":");
    strcat(buffer, len);
    strcat(buffer, ":");
    strcat(buffer, sequence_number);
    buffer[strlen(buffer)] = '\0';

    sendto(broadcaster->sock_fd, buffer, strlen(buffer), 0, (struct sockaddr*) &broadcastaddr, broadcastaddr_len);

    free(msg->payload);
    free(msg);
}

void init_msg_list(MessageList* list){
    list->left = malloc(sizeof(struct MessageList));
    if(list->left == NULL){
        perror("Errore malloc list->left init_msg_list");
        exit(1);
    }

    list->right = malloc(sizeof(struct MessageList));
    if(list->right == NULL){
        perror("Errore malloc list->right init_msg_list");
        exit(1);
    }

    list->msg = malloc(sizeof(struct Message));
    if(list->left == NULL){
        perror("Errore malloc list->msg init_msg_list");
        exit(1);
    }
}

void process_broadcaster(Broadcaster * broadcaster) {
    // In case of packet reception, notify the handler
    // Discard already seen packets
    Message* msg = malloc(sizeof(struct Message));
    if(msg == NULL){
        perror("Errore malloc msg process_broadcaster");
        exit(1);
    }

    MessageList* list = malloc(sizeof(struct MessageList));
    if(list == NULL){
        perror("Errore malloc list process_broadcaster");
        exit(1);
    }
    init_msg_list(list);

    socklen_t sender_len = sizeof(struct sockaddr_in);
    struct sockaddr_in sender_addr;
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    int id, len, sequence_number;
    char* value;

    recvfrom(broadcaster->sock_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&sender_addr, &sender_len);
    buffer[strlen(buffer)] = '\0';

    sscanf(buffer, "%d:%s:%d:%d", &id, value, &len, &sequence_number);
    value[strlen(value)] = '\0';

    memcpy(msg->payload, value, strlen(value));
    msg->payload_lenght = len;
    msg->sequence = sequence_number;
    msg->source = id;

    list->msg = msg;
    while(list->msg != broadcaster->list->msg && broadcaster->list != NULL){
        broadcaster->handler(broadcaster, list->msg->payload);
        if(broadcaster->list->right == NULL){
            list->left = broadcaster->list;
            broadcaster->list->right = list;
        }
        broadcaster->list = broadcaster->list->right;
    }
    free(msg->payload);
    free(msg);
    free(list->left);
    free(list->right);
    free(list->msg->payload);
    free(list->msg);
    free(list);
}

//---------------
// Traffic Analyzer
//---------------

typedef struct TrafficAnalyzer {

    // Definition of sliding window...

} TrafficAnalyzer;

void received_pkt(TrafficAnalyzer * analyzer, int source) {
    // Record the packet send and datetime of it
}

void dump(TrafficAnalyzer * analyzer) {
    // Dump information about the thoughput of all packets received
}

//-------------------------
// Utility
// ------------------------

/**
 * Bind the given socket to all interfaces (one by one)
 * and invoke the handler with same parameter
 */
void bind_to_all_interfaces(int sock, void * context, void (*handler)(int, void *)) {
    struct ifaddrs *addrs, *tmp;
    getifaddrs(&addrs);
    tmp = addrs;
    while (tmp){
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET) {
            setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, tmp->ifa_name, sizeof(tmp->ifa_name));
            handler(sock, context);
        }
        tmp = tmp->ifa_next;
    }
    freeifaddrs(addrs);
}

/**
 * Sleep a given amount of milliseconds
 */
int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do
    {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}


int main() {

    // Autoflush stdout for docker
    setvbuf(stdout, NULL, _IONBF, 0);

    // Traffic generator

    // Broadcaster

    // Traffic analyzer
    return 0;
}