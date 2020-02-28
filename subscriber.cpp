#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helper.h"
#include <netinet/tcp.h>
#include <math.h>

#define BUFFSIZE 1500
#define DELIMS " \n"

int main(int argc, char *argv[])
{
	int sockfd, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFFSIZE];

	/* Preiau argumentele executabilului */
    char *Sclient_id   = argv[1];
    char *Sserver_addr = argv[2];
    char *Sserver_port = argv[3];

    if(strlen(Sclient_id) > 10) {
        perror("ID CLIENT este mai mare de 10 caractere\n");
        return -1;
    }
    char CLIENT_ID[11];
    sprintf(CLIENT_ID, "%s", Sclient_id);

	/* Deschid socket-ul TCP */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	/* Dezactivez algoritmul NEAGLE */
	int yes = 1;
	if (setsockopt(sockfd, SOL_TCP, TCP_NODELAY, &yes, sizeof(int)) < 0) {
        perror("setsockopt(TCP_NODELAY) failed");
	}

	/* Configurari */
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(Sserver_port));
	ret = inet_aton(Sserver_addr, &serv_addr.sin_addr);

	/* Conectarea cu socket-ul server-ului */
	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	if(ret < 0) {close(sockfd); exit(-1);}

	/* Trimit imediat catre server id-ul client-ului */
    ret = send(sockfd, CLIENT_ID, sizeof(CLIENT_ID), 0);
	if(ret < 0) {close(sockfd); exit(-1);}

	/* Resetez file descriptorii */
	fd_set read_fds, tmp_fds;

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);

	int fdmax = sockfd;

	TCP_MSG msg;
	while (1) {
		/* Resetari temporare */
		tmp_fds = read_fds;
		select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		memset(&msg, 0, sizeof(TCP_MSG));

		/* Verific care e socket-ul activ */
		if(FD_ISSET(sockfd, &tmp_fds)) {
			/* Daca este activ cel cu server-ul preiau pachetele */

			/* Verific daca server-ul s-a inchis, caz in care inchid clientul */
			ret = recv(sockfd, &msg, sizeof(TCP_MSG), 0);
			if(msg.type == T_EXIT) {
				break;
			}

			/* Fac parsarea necesara pentru a afisa mesajul trimis de un client UDP */
			UDP_MSG *udp = (UDP_MSG*)msg.payload;
			char tip_date[50];
			char payload[1500];
			switch(udp->tip_date) {
				case 0:
					sprintf(tip_date, "%s", "INT");
					char sign;
					memcpy(&sign, udp->continut, sizeof(char));
					uint32_t nr_netw;
					memcpy(&nr_netw, udp->continut + 1, sizeof(uint32_t));
					nr_netw = ntohl(nr_netw);
					if(sign != 0) nr_netw *= -1;
					sprintf(payload, "%d", nr_netw);
					break;


				case 1:
					sprintf(tip_date, "%s", "SHORT_REAL");
					uint16_t mod;
					mempcpy(&mod, udp->continut, sizeof(uint16_t));
					float float_nr;
					float_nr = (float) ntohs(mod) / 100;
					sprintf(payload, "%.2f", float_nr);
					break;

				case 2:
					sprintf(tip_date, "%s", "FLOAT");
					char sgn;
					uint32_t nr;
					uint8_t mod_putere;
					memcpy(&sgn, udp->continut, sizeof(char));
					memcpy(&nr, udp->continut + 1, sizeof(uint32_t));
					memcpy(&mod_putere, udp->continut + sizeof(char) + sizeof(uint32_t), 
						sizeof(uint8_t));
					nr = ntohl(nr);
					
					float nr_float;
					nr_float = (float) nr * (pow(10, (-1*mod_putere)));

					if(sgn > 0) nr_float *= -1;

					sprintf(payload, "%.4f", nr_float);

					break;


				case 3:
					sprintf(tip_date, "%s", "STRING");
					memset(payload, 0, sizeof(payload));
					sprintf(payload, "%s", udp->continut);
					break;
			}

			printf("%s:%hu - %s - %s - %s\n", udp->ip, udp->port, udp->topic, tip_date, payload);


        }else if(FD_ISSET(STDIN_FILENO, &tmp_fds)) {
			/* Socket-ul este activ pe STDIN, deci voi parsa comenzile primite */
			fgets(buffer, sizeof(buffer), stdin);

            char *token = strtok(buffer, DELIMS);

			if (strcmp(token, "exit") == 0) {
                break;
            }else if(strcmp(token, "subscribe") == 0) {
				char *topic = strtok(NULL, DELIMS);
				if(!topic) continue;

				char *cSF = strtok(NULL, DELIMS);
				if(!cSF) continue;

				char SF = (char)atoi(cSF);

				/* Trimit mesaj server-ului cu comanda */
				TCP_MSG msg;
				msg.type = T_SUBSCRIBE;
				msg.flag = SF;
				msg.size = sizeof(TCP_MSG);
				sprintf(msg.payload, "%s", topic);

				ret = send(sockfd, &msg, sizeof(msg), 0);

				/* Afisez confirmare doar daca s-au trimit > 0 octeti */
				if(ret > 0) {printf("subscribed %s\n", topic);}
            }
			else if(strcmp(token, "unsubscribe") == 0) {
				char *topic = strtok(NULL, DELIMS);
				if(!topic) continue;

				/* Trimit mesaj server-ului cu comanda */
				TCP_MSG tcp_msg;
				tcp_msg.type = T_UNSUBSCRIBE;
				tcp_msg.size = sizeof(TCP_MSG);
				sprintf(tcp_msg.payload, "%s", topic);
				
				ret = send(sockfd, &tcp_msg, sizeof(TCP_MSG), 0);

				/* Afisez confirmare doar daca s-au trimit > 0 octeti */
				if(ret > 0) {printf("unsubscribed %s\n", topic);}
            }
		}

	}

	close(sockfd);
	return 0;
}
