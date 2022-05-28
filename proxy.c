#include  <stdio.h>
#include  <stdlib.h>
#include  <sys/socket.h>
#include  <netdb.h>
#include  <string.h>
#include  <unistd.h>
#include  <stdbool.h>


#define SERVADDR "127.0.0.1"          // Définition de l'adresse IP d'écoute
#define SERVPORT "0"                // Définition du port d'écoute, si 0 port choisi dynamiquement
#define LISTENLEN 1                 // Taille du tampon de demande de connexion
#define MAXBUFFERLEN 1024
#define MAXHOSTLEN 64
#define MAXPORTLEN 6

int connect2Server(const char *serverName, const char *port, int *descSock);

int main(){
    int ecode;                       // Code retour des fonctions
    char serverAddr[MAXHOSTLEN];     // Adresse du serveur
    char serverPort[MAXPORTLEN];     // Port du server
    int descSockRDV;                 // Descripteur de socket de rendez-vous
    int descSockCOM;                 // Descripteur de socket de communication
    struct addrinfo hints;           // Contrôle la fonction getaddrinfo
    struct addrinfo *res;            // Contient le résultat de la fonction getaddrinfo
    struct sockaddr_storage myinfo;  // Informations sur la connexion de RDV
    struct sockaddr_storage from;    // Informations sur le client connecté
    socklen_t len;                   // Variable utilisée pour stocker les 
                     // longueurs des structures de socket
    char buffer[MAXBUFFERLEN];       // Tampon de communication entre le client et le serveur
    // Initialisation de la socket de RDV IPv4/TCP
    descSockRDV = socket(AF_INET, SOCK_STREAM, 0);
    if (descSockRDV == -1) {
         perror("Erreur création socket RDV\n");
         exit(2);
    }
    // Publication de la socket au niveau du système
    // Assignation d'une adresse IP et un numéro de port
    // Mise à zéro de hints
    memset(&hints, 0, sizeof(hints));
    // Initailisation de hints
    hints.ai_flags = AI_PASSIVE;      // mode serveur, nous allons utiliser la fonction bind
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_family = AF_INET;        // seules les adresses IPv4 seront présentées par 
                      // la fonction getaddrinfo

     // Récupération des informations du serveur
     ecode = getaddrinfo(SERVADDR, SERVPORT, &hints, &res);
     if (ecode) {
         fprintf(stderr,"getaddrinfo: %s\n", gai_strerror(ecode));
         exit(1);
     }
     // Publication de la socket
     ecode = bind(descSockRDV, res->ai_addr, res->ai_addrlen);
     if (ecode == -1) {
         perror("Erreur liaison de la socket de RDV");
         exit(3);
     }
     // Nous n'avons plus besoin de cette liste chainée addrinfo
     freeaddrinfo(res);
     // Récuppération du nom de la machine et du numéro de port pour affichage à l'écran

     len=sizeof(struct sockaddr_storage);
     ecode=getsockname(descSockRDV, (struct sockaddr *) &myinfo, &len);
     if (ecode == -1)
     {
         perror("SERVEUR: getsockname");
         exit(4);
     }
     ecode = getnameinfo((struct sockaddr*)&myinfo, sizeof(myinfo), serverAddr,MAXHOSTLEN, 
                         serverPort, MAXPORTLEN, NI_NUMERICHOST | NI_NUMERICSERV);
     if (ecode != 0) {
             fprintf(stderr, "error in getnameinfo: %s\n", gai_strerror(ecode));
             exit(4);
     }
     printf("L'adresse d'ecoute est: %s\n", serverAddr);
     printf("Le port d'ecoute est: %s\n", serverPort);
     // Definition de la taille du tampon contenant les demandes de connexion
     ecode = listen(descSockRDV, LISTENLEN);
     if (ecode == -1) {
         perror("Erreur initialisation buffer d'écoute");
         exit(5);
     }

    len = sizeof(struct sockaddr_storage);

    // Attente connexion du client : boucle infinie du serveur
    while(1) {

        // Lorsque demande de connexion, creation d'une socket de communication avec le client
        descSockCOM = accept(descSockRDV, (struct sockaddr *) &from, &len);
        if (descSockCOM == -1){
            perror("Erreur accept\n");
            exit(6);
        }
        int rapport, numSignal, statut;
        pid_t idProc;

        idProc = fork();
        switch(idProc) {
            case -1:
                perror("Echec fork");
                exit(1);
            case 0:
                close(descSockRDV);
                // TRAITEMENT DU FILS
                printf("Nouvelle connexion au serveur. PID du nouveau processus : %d", getpid());
                // Echange de données avec le client connecté
                strcpy(buffer, "220 BLABLABLA\n");
                write(descSockCOM, buffer, strlen(buffer));

                //Echange de donneés avec le serveur
                ecode = read(descSockCOM, buffer, MAXBUFFERLEN);
                if (ecode == -1) {perror("Problème de lecture\n"); exit(3);}
                buffer[ecode] = '\0';
                printf("MESSAGE RECU DU CLIENT: \"%s\".\n",buffer);

                // Lire USER login@serverName
                char login[50], serverName[50];

                sscanf(buffer, "%49[^@]@%50s", login, serverName);
                strcat(login,"\n");

                printf("login est %s et la machine est %s\n", login, serverName);

                int sockServerCtrl;

                ecode = connect2Server(serverName,"21",&sockServerCtrl);
                if (ecode == -1){
                    perror("Pb connexion avec le serveur FTP");
                    exit(7);
                }

                //S->P : Lire 220 ...
                ecode = read(sockServerCtrl, buffer, MAXBUFFERLEN);
                if (ecode == -1) {perror("Problème de lecture\n"); exit(3);}
                buffer[ecode] = '\0';
                printf("MESSAGE RECU DU SERVEUR: \"%s\".\n",buffer);

                //P->S Envoyer USER ... au serveur
                write(sockServerCtrl, login, strlen(login));

                //S->P : Lire 331
                ecode = read(sockServerCtrl, buffer, MAXBUFFERLEN);
                if (ecode == -1) {perror("Problème de lecture\n"); exit(3);}
                buffer[ecode] = '\0';
                printf("MESSAGE RECU DU SERVEUR: \"%s\".\n",buffer);

                //P->C : 331
                write(descSockCOM, buffer, strlen(buffer));

                //C->P : Lire PASS ...
                ecode = read(descSockCOM, buffer, MAXBUFFERLEN);
                if (ecode == -1) {perror("Problème de lecture\n"); exit(3);}
                buffer[ecode] = '\0';

                //P->S : Envoyer PASS ... au serveur
                write(sockServerCtrl, buffer, strlen(buffer));

                //S->P : Lire 230
                ecode = read(sockServerCtrl, buffer, MAXBUFFERLEN);
                if (ecode == -1) {perror("Problème de lecture\n"); exit(3);}
                buffer[ecode] = '\0';
                printf("MESSAGE RECU DU SERVEUR: \"%s\".\n",buffer);

                //P->C : 230
                write(descSockCOM, buffer, strlen(buffer));

                //C->P : Lire SYST
                ecode = read(descSockCOM, buffer, MAXBUFFERLEN);
                if (ecode == -1) {perror("Problème de lecture\n"); exit(3);}
                buffer[ecode] = '\0';
                printf("MESSAGE RECU DU CLIENT: \"%s\".\n",buffer);


                //P->S : Envoyer SYST
                write(sockServerCtrl, buffer, strlen(buffer));

                //S->P : Lire 215 ...
                ecode = read(sockServerCtrl, buffer, MAXBUFFERLEN);
                if (ecode == -1) {perror("Problème de lecture\n"); exit(3);}
                buffer[ecode] = '\0';
                printf("MESSAGE RECU DU SERVEUR: \"%s\".\n",buffer);

                //P->C : Envoyer 215 ...
                write(descSockCOM, buffer, strlen(buffer));

                //C->P : Lire PORT a,b,c,d,e,f
                char* a[4];
                char* b[4];
                char* c[4];
                char* d[4];
                int e,f,g;
                char* port[10];
                ecode = read(descSockCOM, buffer, MAXBUFFERLEN);
                if (ecode == -1) {perror("Problème de lecture\n"); exit(3);}
                buffer[ecode] = '\0';
                printf("MESSAGE RECU DU CLIENT: \"%s\".\n",buffer);
                sscanf(buffer, "PORT %[0123456789],%[0123456789],%[0123456789],%[0123456789],%d,%d", a, b, c, d, &e, &f);
            
                // Créer serverName = a.b.c.d et port = 256 * e + f
                strcpy(serverName, a);
                strcat(serverName, ".");
                strcat(serverName, b);
                strcat(serverName, ".");
                strcat(serverName, c);
                strcat(serverName, ".");
                strcat(serverName, d);
                g = e*256 + f;
                sprintf(port, "%d", g);
                printf("Server name = %s, Port = %s\n", serverName, port); // AAAAAA

                // Création de la connection passive entre C et P
                int socketPassive;

                ecode = connect2Server(serverName, port, &socketPassive);

                if (ecode == -1){
                    perror("Pb connexion avec le client");
                    exit(7);
                }

                // P->S : Envoyer PASV
                write(sockServerCtrl, "PASV\r\n", strlen("PASV\r\n"));

                // S->P : Lire 227 Entering Passive Mode a,b,c,d,e,f
                ecode = read(sockServerCtrl, buffer, MAXBUFFERLEN);
                if (ecode == -1) {perror("Problème de lecture\n"); exit(3);}
                buffer[ecode] = '\0';
                printf("MESSAGE RECU DU SERVEUR: \"%s\".\n",buffer);
                sscanf(buffer, "227 Entering Passive Mode (%[0123456789],%[0123456789],%[0123456789],%[0123456789],%d,%d)", a, b, c, d, &e, &f);

                // Créer serverName = a.b.c.d et port = 256 * e + f
                strcpy(serverName, a);
                strcat(serverName, ".");
                strcat(serverName, b);
                strcat(serverName, ".");
                strcat(serverName, c);
                strcat(serverName, ".");
                strcat(serverName, d);
                g = e*256 + f;
                sprintf(port, "%d", g);
                printf("Server name = %s, Port = %s\n", serverName, port);

                // Création de la connection active entre P et S
                int socketActive;

                ecode = connect2Server(serverName, port, &socketActive);

                if (ecode == -1){
                    perror("Pb connexion avec le serveur");
                    exit(7);
                }

                //P->C : 200
                write(descSockCOM, "200 Connecté avec succès\n", strlen("200 Connecté avec succès\n"));
                
                //C->P : Lire LIST
                ecode = read(descSockCOM, buffer, MAXBUFFERLEN);
                if (ecode == -1) {perror("Problème de lecture\n"); exit(3);}
                buffer[ecode] = '\0';
                printf("MESSAGE RECU DU CLIENT: \"%s\".\n",buffer);

                //P->S : Envoyer LIST
                write(sockServerCtrl, buffer, strlen(buffer));

                //S->P : Lire la réponse 150 opening ...
                ecode = read(sockServerCtrl, buffer, MAXBUFFERLEN);
                if (ecode == -1) {perror("Problème de lecture\n"); exit(3);}
                buffer[ecode] = '\0';
                printf("MESSAGE RECU DU SERVEUR: \"%s\".\n",buffer);

                //P->C : Envoyer 150 ...
                write(descSockCOM, buffer, strlen(buffer));

                while(strcmp(buffer, "")) {
                    //S->P : Lire le résultat de LIST
                    ecode = read(socketActive, buffer, MAXBUFFERLEN);
                    if (ecode == -1) {perror("Problème de lecture\n"); exit(3);}
                    buffer[ecode] = '\0';
                    printf("MESSAGE RECU DU SERVEUR: \"%s\".\n",buffer);

                    //P-> : Envoyer le résultat de LIST
                    write(socketPassive, buffer, strlen(buffer)); 
                }

                //P->C : Envoyer 226 Transfer complete
                write(descSockCOM, "226 Transfer complete\n", strlen("226 Transfer complete\n"));

                //Fermeture de la connexion
                close(descSockCOM);
                close(socketActive);
                close(socketPassive);
                printf("Fin du processus de PID : %d", getpid());

                // FIN TRAITEMENT DU FILS
                exit(1);
            break;
        }
    }  
}

int connect2Server(const char *serverName, const char *port, int *descSock){

    int ecode;                     // Retour des fonctions
    struct addrinfo *res,*resPtr;  // Résultat de la fonction getaddrinfo
    struct addrinfo hints;
    
    bool isConnected = false;      // booléen indiquant que l'on est bien connecté

    // Initailisation de hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_family = AF_UNSPEC;      // les adresses IPv4 et IPv6 seront présentées par 
                                          // la fonction getaddrinfo

    //Récupération des informations sur le serveur
    ecode = getaddrinfo(serverName,port,&hints,&res);
    if (ecode){
        fprintf(stderr,"getaddrinfo: %s\n", gai_strerror(ecode));
        return -1;
    }

    resPtr = res;
/*
    int val;
    valeur -> val
    adresse de val -> &val

    int *vaux;
    valeur -> *vaux
    adresse -> vaux
*/
    while(!isConnected && resPtr!=NULL){

        //Création de la socket IPv4/TCP
        *descSock = socket(resPtr->ai_family, resPtr->ai_socktype, resPtr->ai_protocol);
        if (*descSock == -1) {
            perror("Erreur creation socket");
            return -1;
        }

        //Connexion au serveur
        ecode = connect(*descSock, resPtr->ai_addr, resPtr->ai_addrlen);
        if (ecode == -1) {
            resPtr = resPtr->ai_next;           
            close(*descSock);   
        }
        
        // On a pu se connecté
        else isConnected = true;
    }
    freeaddrinfo(res);
    if (!isConnected){
        perror("Connexion impossible");
        return -1;
    }
    
    return 0;
}