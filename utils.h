/*

** Fichero: utils.h
** Autores: 
** Fernando Olivares Naranjo DNI 54126671N
** Diego Sánchez Martín DNI 54126671N
*/

#include <sys/types.h>
#include <sys/socket.h>


void logPeticiones(struct sockaddr_in clientaddr_in, char * tipo, char * buffer);