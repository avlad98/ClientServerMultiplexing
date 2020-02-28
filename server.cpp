#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helper.h"
#include <iterator>
#include <vector>
#include <netinet/tcp.h>

using namespace std;

#define BUFFSIZE 2000
#define MAX_CLIENTS 100
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

/* Elibereaza memoria alocata pentru vectorul de clienti impreuna cu */
/* mesajele in curs de trimitere si topic-urile stocate */
void freeClients(vector<CLIENT*> clients) {
	for(auto client = clients.begin(); client != clients.end(); client++) {
		vector<TCP_MSG*> pending = (*client)->pending;
		vector<TOPIC*> topics = (*client)->topics;

		for(auto pend = pending.begin(); pend != pending.end(); pend++) {
			free(*pend);
		}

		for(auto topic = topics.begin(); topic != topics.end(); topic++) {
			free(*topic);
		}

		free(*client);
	}
}

int main(int argc, char *argv[]) {

    int tcp_sock, udp_sock, curr_sock, newsockfd, port, ret;
	char buffer[BUFFSIZE];
	struct sockaddr_in serv_addr_tcp, cli_addr_tcp;
	struct sockaddr_in serv_addr_udp, cli_addr_udp;
	socklen_t clilen;

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	/* Resetez descriptorii */
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	/* Preiau portul din lista de argumente */
	port = atoi(argv[1]);

	/* Deschid socketii de TCP si UDP */
	tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(tcp_sock < 0) {printf("Eroare tcp_sock\n"); return -1;}

    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp_sock < 0) {printf("Eroare udp_sock\n"); close(tcp_sock); return -1;}

	/* TCP */
	memset((char *) &serv_addr_tcp, 0, sizeof(serv_addr_tcp));
	serv_addr_tcp.sin_family = AF_INET;
	serv_addr_tcp.sin_port = htons(port);
	serv_addr_tcp.sin_addr.s_addr = INADDR_ANY;

	/* UDP */
	memset((char *) &serv_addr_udp, 0, sizeof(serv_addr_udp));
	serv_addr_udp.sin_family = AF_INET;
	serv_addr_udp.sin_port = htons(port);
	serv_addr_udp.sin_addr.s_addr = INADDR_ANY;

	/* Util pentru DEBUG sa nu astept 1-2 min pentru a deschide SO port-ul vechi */
	int yes = 1;
	if (setsockopt(tcp_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
    }

	/* BIND + LISTEN pe socket-ul de TCP */
	ret = bind(tcp_sock, (struct sockaddr *) &serv_addr_tcp, sizeof(struct sockaddr));
    if(ret < 0) {
        perror("Eroare bind tcp_sock\n"); 
        close(tcp_sock); 
        close(udp_sock);
        return -1;
    }
	ret = listen(tcp_sock, MAX_CLIENTS);
    if(ret < 0) {
        printf("Eroare listen tcp_sock\n"); 
        close(tcp_sock); 
        close(udp_sock);
        return -1;
    }

	/* Doar BIND pe socket-ul de UDP */
	ret = bind(udp_sock, (struct sockaddr *) &serv_addr_udp, sizeof(struct sockaddr));
    if(ret < 0) {
        perror("Eroare bind udp_sock\n"); 
        close(tcp_sock); 
        close(udp_sock);
        return -1;
    }

	/* se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds */
	FD_SET(tcp_sock, &read_fds);
	FD_SET(udp_sock, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

	/* Actualizez socket-ul cu ID-ul cel mai mare (limita superioara) */
	fdmax = MAX(tcp_sock, udp_sock);

	/* In acest vector retin clientii conectati macar o data */
	vector<CLIENT*> clients;

	while (1)
	{
		tmp_fds = read_fds; 
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        if(ret < 0) {
            printf("Eroare select\n");
            close(tcp_sock);
            close(udp_sock);
            return -1;
        }

		/* Ma plimb prin toti socketii sa vad care este cel curent activ */
		for (curr_sock = 0; curr_sock <= fdmax; curr_sock++) {
			if (FD_ISSET(curr_sock, &tmp_fds)) {

                /* Verific daca se citeste de la STDIN */
                if(curr_sock == STDIN_FILENO) {
                    char command[50];
                    scanf("%s", command);

                    /* Daca s-a introdus comanda exit inchid socketii */
                    /* si conexiunile cu clientii */
                    if(strcmp(command, "exit") == 0) {

                        /* BROADCAST CU CODUL DE EXIT */
						for(auto itr = clients.begin(); itr != clients.end(); itr++) {
							if((*itr)->on == ONLINE) {
								TCP_MSG m_exit;
								m_exit.type = T_EXIT;

								do {ret = send((*itr)->socket, &m_exit, sizeof(TCP_MSG), 0);}
								while(ret < 0);
							}
						}

						/* Dezaloc toata memoria alocata */
						freeClients(clients);

                        close(tcp_sock);
                        close(udp_sock);
                        return 0;
                    }

                    /* Daca s-a introdus altceva serverul continua activitatea */
                    continue;
                }

				/* Un client doreste sa se conecteze pe socket-ul serverului */
				if (curr_sock == tcp_sock) {

					/* Preiau detaliile clientului */
					clilen = sizeof(cli_addr_tcp);
					newsockfd = accept(tcp_sock, (struct sockaddr *) &cli_addr_tcp, &clilen);

                    ret = recv(newsockfd, buffer, BUFFSIZE, 0);
                    if(ret < 0) {perror("Eroare recv\n"); return -1;}
                    
					/* Aloc o structura pentru clientul primit */
					CLIENT *client = (CLIENT*)calloc(1, sizeof(CLIENT));
					if(!client) {perror("Eroare alocare structura client!\n"); continue;}

                    sprintf(client->id, "%s", buffer);
                    sprintf(client->ip, "%s", inet_ntoa(cli_addr_tcp.sin_addr));
                    client->port = ntohs(cli_addr_tcp.sin_port);
                    client->on = ONLINE;
					client->socket = newsockfd;

					bool found = false;
					bool skip = false;

					/* Caut daca acest client a mai fost conectat */
					for(auto itr = clients.begin(); itr != clients.end(); itr++) {
						if((strcmp((*itr)->id, client->id) == 0)) { 
							/* se incearca conectarea unui client cu acelasi id */
							/* peste cel deja conectat... se va ignora cererea */
							if((*itr)->on == ONLINE) {
								TCP_MSG m_exit;
								m_exit.type = T_EXIT;

								do {ret = send(newsockfd, &m_exit, sizeof(TCP_MSG), 0);}
								while(ret < 0);

								free(client);
								skip = true;
								continue;
							}else {
								/* clientul se reconecteaza la server.. se actualizeaza datele*/
								sprintf((*itr)->ip, "%s", client->ip);
								(*itr)->port = client->port;
								(*itr)->on = ONLINE;
								(*itr)->socket = client->socket;

								/* Se trimit mesajele din pending */
								auto m = (*itr)->pending;
								while(!m.empty()) {
									ret = send((*itr)->socket, m.front(), sizeof(TCP_MSG), 0);
									if(ret > 0) {
										m.erase(m.begin());
									}
								}

								found = true;
							}
						}					
					}

					if(skip) continue;

					/* Daca ajunge aici inseamna ca este un client nou */
                    printf("New client (%s) connected from %s:%hu\n", 
                        client->id,
                        client->ip,
                        client->port);

					if(!found) {
						/* clientul se conecteaza prima data la server */
						clients.push_back(client);
					}else {
						/* Daca acest client se reconecteaza eliberez memoria nou */
						/* alocata pentru el */
						free(client);
					}


					/* Se adauga noul socket intors de accept() la multimea descriptorilor de citire */
					FD_SET(newsockfd, &read_fds);

					/* Update cel mai mare socket */
					if (newsockfd > fdmax) {fdmax = newsockfd;}
				}else if(curr_sock == udp_sock){
					/* Un client UDP incearca sa se conecteze */

					/* Preiau datele intr-o structura ca cea din PDF */
					UDP_MSG udp_msg;
					clilen = sizeof(cli_addr_udp);
					ret = recvfrom(udp_sock, &udp_msg, sizeof(UDP_MSG), 0, (struct sockaddr *)&cli_addr_udp, &clilen);
					if(ret < 0) continue;

					sprintf(udp_msg.ip, "%s", inet_ntoa(cli_addr_udp.sin_addr));
					udp_msg.port = ntohs(cli_addr_udp.sin_port);

					/* Verific fiecare utilizator daca este abonat la topic-ul curent */
					for(auto itr = clients.begin(); itr != clients.end(); itr++) {
						bool subscribed = false;
						bool SF = false;

						/* Caut topic-ul in lista de topic-uri */
						for(auto topic = (*itr)->topics.begin(); topic != (*itr)->topics.end(); topic++) {
							if(strcmp((*topic)->topic, udp_msg.topic) == 0) {
								subscribed = true;
								SF = (*topic)->SF;
								break;
							}
						}

						/* Trimit mesaj doar daca e abonat */
						if(subscribed) {
							TCP_MSG toSend;
							toSend.type = T_TCP;
							toSend.size = sizeof(TCP_MSG);
							memcpy(toSend.payload, &udp_msg, sizeof(UDP_MSG));

							/* Daca clientul e online ii trimit topic-ul */
							if((*itr)->on == ONLINE) {
								ret = send((*itr)->socket, &toSend, sizeof(TCP_MSG), 0);
							}
							else if(SF) {
								/* Daca nu e, verific daca are activat SF si introduc in coada */
								TCP_MSG *tmp = (TCP_MSG*)calloc(1, sizeof(TCP_MSG));
								if(!tmp) continue;
								tmp->size = sizeof(TCP_MSG);
								tmp->type = T_TCP;
								memcpy(tmp->payload, &udp_msg, sizeof(UDP_MSG));

								(*itr)->pending.push_back(tmp);
							}
							/* Altfel nu trimit */
						}

					}
				}else{
					/* Unul din clienti a trimis date (SUBSCRIBE/UNSUBSCRIBE/EXIT) */
					memset(buffer, 0, BUFFSIZE);
					ret = recv(curr_sock, buffer, BUFFSIZE, 0);
                    if(ret < 0) {perror("Mesaj client"); continue;}

					/* EXIT */
					if (ret == 0) {
						/* Clientul s-a deconectat */

						/* Caut clientul in lista si ii aflu ID-ul */
						for(auto itr = clients.begin(); itr != clients.end(); itr++){
							if((*itr)->socket == curr_sock) {
								(*itr)->on = OFFLINE;
								printf("Client (%s) disconnected\n", (*itr)->id);
								break;
							}
						}

						close(curr_sock);
						
						/* Scot din multimea de citire socketul inchis */
						FD_CLR(curr_sock, &read_fds);
					} else {
						/* SUBSCRIBE / UNSUBSCRIBE */

						TCP_MSG msg;
						memcpy(&msg, buffer, sizeof(TCP_MSG));
						if(ret < 0){perror("Subscribe / Unsubscribe"); continue;}

						/* Caut clientul in vector */
						for(auto itr = clients.begin(); itr != clients.end(); itr++){
							
							/* Clientul s-a gasit */
							if((*itr)->socket == curr_sock) {
								/* Verific SUBSCRIBE/UNSUBSCRIBE */

								/* Daca era deja subscriber la topic-ul curent decat */
								/* Actualizez SF-ul */
								if(msg.type == T_SUBSCRIBE) {
									for(auto m = (*itr)->topics.begin(); m != (*itr)->topics.end(); m++) {
										if(strcmp((*m)->topic, msg.payload) == 0) {
											(*m)->SF = msg.flag;
											continue;
										}
									}

									/* Daca topicul nu exista il adaug */
									TOPIC *topic = (TOPIC*)calloc(1, sizeof(TOPIC));
									if(!topic) {perror("Eroare alocare spatiu pentru topic\n"); continue;}
									sprintf(topic->topic, "%s", msg.payload);
									topic->SF = msg.flag;

									(*itr)->topics.push_back(topic);
								}else if(msg.type == T_UNSUBSCRIBE) {
									vector<TOPIC*> topics = (*itr)->topics;
									vector<TCP_MSG*> pending = (*itr)->pending;

									/* Sterg topic-ul la care s-a dezabonat clientul */
									for(auto iter = topics.begin(); iter != topics.end(); iter++) {
										if(strcmp((*iter)->topic, msg.payload) == 0) {
											TOPIC *tmp = *iter;
											topics.erase(iter);
											free(tmp);
											break;
										}
									}
								}								
							}
						}

					}
				}
			}
		}
	}


	freeClients(clients);
	close(tcp_sock);
    close(udp_sock);

    return 0;
}
