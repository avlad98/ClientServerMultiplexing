#include <iostream>
#include <vector>
using namespace std;

#define ONLINE 1
#define OFFLINE 0

#define SF_ON 1
#define SF_OFF 0

#define T_UDP 10
#define T_TCP 11
#define T_EXIT 12
#define T_SUBSCRIBE 13
#define T_UNSUBSCRIBE 14

/* Structura in care tin minte daca este abonat cu SF */
/* la un topic anume */
typedef struct {
    char topic[50];
    bool SF;
} TOPIC;

/* Structura in care primesc mesajele de la clienti UDP + IP si PORT */
typedef struct {
    char topic[50];
    char tip_date;
    char continut[1500];
    char ip[11];
    unsigned short port;
} UDP_MSG;


/* Structura prin care comunic cu serverul si clientii TCP */
/* In payload am loc pentru un pachet de UDP pentru a-l parsa in client */
typedef struct {
    char payload[sizeof(UDP_MSG)];
    char type;
    char flag;
    unsigned int size;
} TCP_MSG;

/* Structura in care tin minte un client care s-a logat si subscribtiile */
/* acestuia impreuna cu alte informatii aditionale */
typedef struct {
    int socket;
    unsigned short port;
    char on;
    char id[11];
    char ip[50];
    std::vector<TCP_MSG*> pending;
    std::vector<TOPIC*> topics;
} CLIENT;