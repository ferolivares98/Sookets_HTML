/*

** Fichero: utils.c
** Autores: 
** Fernando Olivares Naranjo DNI 54126671N
** Diego Sánchez Martín DNI 44435121H
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <unistd.h>

#include "utils.h"


void logPeticiones(struct sockaddr_in clientaddr_in, char * tipo, char * buffer){
    FILE *f;
    char cadenaLog[150];
    //char servname[NI_MAXSERV];
    char hostname[NI_MAXHOST];
    char puerto[10];
    long tiempo;

    strcpy(cadenaLog, "");

    time(&tiempo);
    strcat(cadenaLog, (char *)ctime(&tiempo));
    strcat(cadenaLog, "\t");
    //strcat(cadenaLog, "IP: ");
    //getnameinfo((struct sockaddr *)&clientaddr_in, sizeof(clientaddr_in), NULL, 0, servname, NI_MAXSERV, 0);
    //strcat(cadenaLog, servname);
    //strcat(cadenaLog, " ");
    strcat(cadenaLog, "HOST: ");
    getnameinfo((struct sockaddr *)&clientaddr_in, sizeof(clientaddr_in), hostname, NI_MAXHOST, NULL, 0, 0);
    strcat(cadenaLog, hostname);
    strcat(cadenaLog, " | ");
    strcat(cadenaLog, tipo);
    strcat(cadenaLog, " en puerto: ");
    sprintf(puerto, "%d", ntohs(clientaddr_in.sin_port));
    strcat(cadenaLog, puerto);
    strcat(cadenaLog, " | Mensaje: ");
    strcat(cadenaLog, buffer);

    if((f = fopen("peticiones.log", "a")) == NULL){
        printf("Error acceso peticiones.log\n");
    }else{
        fprintf(f, "%s\n", cadenaLog);
        fclose(f);
    }
}


void escribirFicheroPuEfi(char * buffer, struct sockaddr_in myaddr_in){
    FILE *f;
    char puerto[10];
    char ficheroPuEfi[25];

    sprintf(puerto, "%d", ntohs(myaddr_in.sin_port));
    strcpy(ficheroPuEfi, "salidaPuerto/");
    strcat(ficheroPuEfi, puerto);
    strcat(ficheroPuEfi, ".txt");

    if((f = fopen(ficheroPuEfi, "a")) == NULL){
        printf("Error acceso fichero de puerto efImero.");
    }else{
        fprintf(f, "%s", buffer);
        fclose(f);
    }
}