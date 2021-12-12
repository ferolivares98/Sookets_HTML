/*

** Fichero: utils.c
** Autores: 
** Fernando Olivares Naranjo DNI 54126671N
** Diego Sánchez Martín DNI 54126671N
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


void logPeticiones(struct sockaddr_in clientaddr_in, char * tipo, char * buffer){
    FILE *f;
    char cadenaLog[150];
    long tiempo;

    strcpy(cadenaLog, "");

    time(&tiempo);
    strcat(cadenaLog, (char *)ctime(&tiempo));
    strcat(cadenaLog, "\t");
    strcat(cadenaLog, tipo);
    strcat(cadenaLog, "en puerto: ");
    strcat(cadenaLog, ntohs(clientaddr_in.sin_port));
    strcat(cadenaLog, " Mensaje: ");
    strcat(cadenaLog, buffer);

    if((f = fopen("peticiones.log", "a")) == NULL){
        printf("Error acceso peticiones.log\n");
    }else{
        fprintf(f, "%s\n", cadenaLog);
        fclose(f);
    }
}