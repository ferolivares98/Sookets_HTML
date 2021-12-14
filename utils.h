/*

** Fichero: utils.h
** Autores: 
** Fernando Olivares Naranjo DNI 54126671N
** Diego Sánchez Martín DNI 44435121H
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


void logPeticiones(struct sockaddr_in clientaddr_in, char * tipo, char * buffer);
void escribirFicheroPuEfi(char * buffer, struct sockaddr_in myaddr_in);