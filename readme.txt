VLAD ANDREI-ALEXANDRU
321CB

================================== TEMA 2 PC ==================================

	Am implementat aceasta tema in C++ pentru usurinta folosirii structurilor
de date deja existente in STL.

	Tema contine 3 fisiere:
	- server.cpp
	- subscriber.cpp
	- helper.h


	helper.h
-----------------------------
	
	In acest fisier header mi-am declarat structurile de care am avut nevoie
petru a stoca informatii despre si de la clienti (TCP + UDP).
	
	TOPIC -> structura in care tin minte daca este abonat cu SF la un topic
			anume.

	UDP_MSG -> structura in care primesc mesajele de la clienti UDP + IP 
			si PORT

	TCP_MSG -> Structura prin care comunic cu serverul si clientii TCP.
		In payload am loc pentru un pachet de UDP pentru a-l parsa in client.

	CLIENT -> Structura in care tin minte un client care s-a logat si 
		subscribtiile acestuia impreuna cu alte informatii aditionale.

	+ alte macro-uri pentru flag-uri


	subscriber.cpp
-----------------------------

	In subscriber preiau informatiile date ca argument executabilului,
deschid socket-ul TCP prin care comunic cu server-ul si dezactivez
algoritmul lui Neagle. Dupa acestea fac configurarile structurilor
de retea (ca la laboratoarele 6, 7, 8) si ma conectez la server.
	Daca nu se reuseste conectarea atunci inchid clientul pe loc.
	Dupa ce se conecteaza trimit imediat id-ul clientului curent.
	Daca acest transfer nu reuseste inchid imediat client-ul.
	Dupa toate configurarile necesare verific intr-un while infinit
care este socket-ul curent si execut actiunile corespunzatoare.
	Daca este activ STDIN parsez instructiunea si o trimit catre
server printr-o structura de tip TCP_MSG. Daca instructiunea nu este
scrisa valid se ignora acea linie.
	Daca este activ pe FD-ul serverului preiau pachetul de la acesta
(tot de tip TCP_MSG) si parsez pachetul UDP din payload-ul mesajului
principal daca acesta este de tip T_TCP.
	Daca pachetul este de tip T_EXIT atunci clientul se va inchide
(adica serverul a trimis broadcast cu codul de EXIT).
	In functie de tipul de date afisez corespunzator (asa cum este
explicat in PDF).


	server.cpp
-----------------------------

	In server preiau portul dat ca argument, deschid socketii
pentru TCP si UDP si fac configuratiile necesare ca la laboratoarele
6, 7 si 8.
	Folosesc setsockopt pentru a nu sta sa astept mereu 1-2 minute pana
se redeschide portul.
	Fac BIND si LISTEN pe socket-ul TCP si pe cel de UDP doar BIND,
adaug file descriptorii in multimea de FD-s care comuta.
	Creez un vector de CLIENT* in care aloc o structura pentru fiecare
client conentat unic (cei care se reconecteaza inca au structura salvata
in vector pana la inchiderea serverului).
	In bucla while infinita verific care e socket-ul activ si tratez cazurile
fiecaruia.

	Daca socket-ul curent este STDIN (de fapt file_descriptor-ul curent)
verific daca s-a primit comanda exit. Daca da, atunci fac broadcast cu 
pachetul de exit. Daca s-a introdus altceva inafara de exit va fi ignorat.
	Daca cel curent este socket-ul TCP inseamna ca un client doreste sa se
conecteze. Preiau detaliile acestuia, ii aloc o structura si o inserez in
vector doar daca acesta s-a conectat prima data la server. Daca acest client
incearca sa se conecteze cu acealasi ID ca unul din cei ONLINE atunci acesta
va fi dat afara (trimite pachet-ul exit). Daca un client incearca sa se 
reconecteze se vor actualiza informatiile cele vechi cu cele noi si se va
dezaloca structura de CLIENT alocata anterior (nu va mai fi nevoie de ea
pentru ca o actualizez pe cea veche). De asemenea atunci cand un client
se reconecteaza, acesta va primi mesajele din vectorul pending (cele la care
avea subscriptie cu SF 1 si nu le-a primit). Acestea vor fi incarcate mereu
la fiecare reconectare a sa pentru a nu pierde nicio informatie.
	
	Daca un client UDP incearca sa se conecteze acestuia i se vor primi datele,
se verifica pentru fiecare client daca este abonat la acest topic primit si i
se va trimite acest pachet de UDP (printr-un mesaj TCP_MSG) doar daca acesta
este online. Daca nu este online si are SF activat pentru acest topic, mesajul
i se va adauga in lista de pending, urmand sa fie trimis la reconectare. Daca
nu este nici activat SF-ul sau nu este abonat la topic se va ignora clientul.
	
	Daca este alt socket activ atunci un client a trimis o comanda de 
SUBSCRIBE / UNSUBSCRIBE / EXIT. Caut acest client in lista dupa socket si
verific ce comanda a trimis. Daca este exit atunci se va afisa mesajul de
deconectare si se va elimina socket-ul din cele curente. Daca este SUBSCRIBE
atunci caut topicul in lista sa si ii actualizez SF-ul daca exista. Daca nu
exista inserez topicul in lista cu SF-ul corespunzator.
	Daca se cere UNSUBSCRIBE elimin din lista topicul corespunzator.
	La aceste eliminari din liste fac si dezalocare pentru a nu avea scurgeri
de memorie.


TESTE:
	La comanda exit dintr-un client acesta se inchide, iar serverul 
receptioneaza si afiseaza mesajul cu Disconnect...
	La comanda exit din server se inchid toti clientii si serverul.
	Comenzile subscribe si unsubscribe functioneaza pentru toti clientii
activi. Cand se face o subscriptie cu SF 1 acesta va primi la reconectare
toate mesajele care au fost trimise de clientii UDP la care era abonat.
	La unsubscribe nu se vor mai primi pachete pentru acel topic.
	In subscriber se afiseaza pachetele parsate asa cum este explicat in PDF.
