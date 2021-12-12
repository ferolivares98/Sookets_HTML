/*

** Fichero: servidor.c
** Autores:
** Fernando Olivares Naranjo DNI 54126671N
** Diego Sánchez Martín DNI 54126671N
*/


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "utils.h"

#define PUERTO 5121
#define ADDRNOTFOUND	0xffffffff	/*Dirección de retorno de host no encontrado.*/
#define BUFFERSIZE	1024	/*Tamaño máximo de los paquetes recibidos.*/
#define MAXHOST 128

void serverTCP(int s, struct sockaddr_in peeraddr_in);
void serverUDP(int s, char * buffer, struct sockaddr_in clientaddr_in);
void errout(char *);

int FIN = 0;
void finalizar(){ FIN =1; }
void alarma(){ return; }

int main(int argc, char * argv){

    int s_TCP, s_UDP;   /*Descriptor de socket conectado*/
    int ls_TCP;         /*Descriptor del listen socket (TCP)*/

    int cc;

    struct sigaction sa = {.sa_handler = SIG_IGN};

    struct sockaddr_in myaddr_in;
    struct sockaddr_in clientaddr_in;
    int addrlen;

    fd_set readmask;
    int numfds, s_mayor;

    char buffer[BUFFERSIZE];

    struct sigaction vec;

    /*Crear el listen socket.*/
    ls_TCP = socket(AF_INET, SOCK_STREAM, 0);
    if (ls_TCP == 1){
        perror(argv[0]);
        fprintf(stderr, "%s: unable to create socket TCP\n", argv[0]);
        exit(1);
    }

    /*Limpiar las estructuras de las direcciones.*/
    memset ((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
    memset ((char *)&clientaddr_in, 0, sizeof(struct sockaddr_in));
    addrlen = sizeof(struct sockaddr_in);

    /*Estructura de direcciones para el listen socket.*/
    myaddr_in.sin_family = AF_INET;
    /*El servidor debería escuchar en el wildcar address.
    **Buena práctica en los servidores. Server escuchando a múltiples
    **networks al mismo tiempo.*/
    myaddr_in.sin_addr.s_addr = INADDR_ANY;
    myaddr_in.sin_port = htons(PUERTO);


    /*Bind (TCP) la dirección de listen con el socket*/
    if(bind(ls_TCP, (const struct sockaddr *) &myaddr_in, sizeof(struct sockaddr_in)) == -1){
        perror(argv[0]);
        fprintf(stderr, "%s: unable to bind adress TCP\n", argv[0]);
        exit(1);
    }
    /*Listen necesario (TCP) para la conexión de usuarios remotos.*/
    if(listen(ls_TCP, 5) == -1){
        perror(argv[0]);
        fprintf(stderr, "%s: unable to listen on socket\n", argv[0]);
        exit(1);
    }

    /*crear el socket UDP.*/
    s_UDP = socket(AF_INET, SOCK_DGRAM, 0);
    if(s_UDP == -1){
        perror(argv[0]);
        printf("%s: unable to create socket UDP\n");
        exit(1);
    }
    /*Bind (UDP) la dirección de listen con el socket.*/
    if(bind(s_UDP, (const struct sockaddr *) &myaddr_in, sizeof(struct sockaddr_in)) == -1){
        perror(argv[0]);
        fprintf(stderr, "%s: unable to bind adress UDP\n", argv[0]);
        exit(1);
    }


    /*Una vez realizados los pasos y comprobaciones anteriores, se prepara
    **el servidor para trabajar como un daemon. Debemos usar setpgrp para
    **que deje de estar asociado con el terminal. El padre debe hacer el pgrp
    **para evitar que los hijos (posteriores al fork) puedan llegar a controlar
    **un terminal.*/
   setpgrp();

   switch(fork()){
        case -1:
            perror(argv[0]);
            fprintf(stderr, "%s: unable to fork daemon\n", argv[0]);
            exit(1);

        case 0:
            /*Cierre de stdin y stdout, innecesarios de ahora en adelante.
            **El daemon no reportará errores.*/
            fclose(stdin);
            fclose(stderr);

            /*SIGCLD a SIG_IGN para prevenir la acumulación de zombies cuando
            los hijos terminen*. Evita llamadas a wait.*/
            if(sigaction(SIGCHLD, &sa, NULL) == -1){
                perror("sigaction(SIGCHLD");
                fprintf(stderr, "%s: unable to register the SIGCHLD signal\n", argv[0]);
                exit(1);
            }

            /*Registrar SIGTERm para finalización ordenada del servidor.*/
            vec.sa_handler = (void *) finalizar;
            vec.sa_flags = 0;
            if(sigaction(SIGTERM, &vec, (struct sigaction *) 0) == -1){
                perror("sigaction(SIGTERM");
                fprintf(stderr, "%s: unable to register the SIGTERM signal\n", argv[0]);
                exit(1);
            }

            while(!FIN){
                /*Meter en el conjunto de sockets los sockets de UDP y TCP*/
                FD_ZERO(&readmask);
                FD_SET(ls_TCP, &readmask);
                FD_SET(s_UDP, &readmask);

                /*Seleccionarl el descriptor del socket que ha cambiado. Deja una marca
                **en el conjutno de sockets 9readmask)*/
                if(ls_TCP > s_UDP) s_mayor = ls_TCP;
                else s_mayor = s_UDP;

                if ((numfds = select(s_mayor+1, &readmask, (fd_set *)0, (fd_set *)0, NULL)) < 0){
                    if(errno == EINTR){
                        FIN=1;
                        close(ls_TCP);
                        close(s_UDP);
                        perror("\nFinalizando el servidor. Señal recibida en elect\n");
                    }
                }else{
                    /*Comprobar si el socket seleccionado es TCP*/
                    if(FD_ISSET(ls_TCP, &readmask)){
                        /*addrlen como puntero para que la llamada que acepta pueda devolver el tamaño de la
                        **dirección de retorno. Esta llamada se bloquea hasta que llga una nueva conexión.
                        **Devolvera la dirección del "conectado" y un nuevo descriptor.*/
                       s_TCP = accept(ls_TCP, (struct sockaddr *) &clientaddr_in, &addrlen);
                       if(s_TCP == -1) exit(1);
                       switch(fork()){
                            case -1:  /*No puede fork, sale.*/
                                exit(1);
                            case 0:  /*Child aquí.*/
                                close(ls_TCP);  /*Cierra el listen socket heredado del daemon*/
                                serverTCP(s_TCP, clientaddr_in);
                                exit(0);
                            default:
                                /*El daemon debe cerrar el nuevo socket aceptado después del fork. Permitirá que
                                **el daemon no se quede sin espacio de fd y que en el futuro se pueda destruit el
                                **socket al cerrarlo.*/
                               close(s_TCP);
                       }
                    }  /*De TCP*/
                    /*Comprobar si el socket seleccionado es el UDP.*/
                    if(FD_ISSET(s_UDP, &readmask)){
                        /*Se bloqueará hasta que llegue una nueva petición. Devolverá la dirección del cliente y
                        **un buffer con la petición. El último caracter del buffer será null.*/
                       cc = recvfrom(s_UDP, buffer, BUFFERSIZE - 1, 0, (struct sockaddr *)&clientaddr_in, &addrlen);
                       if(cc == -1){
                           perror(argv[0]);
                           printf("%s: recvfrom error\n", argv[0]);
                           exit(1);
                       }

                       /*Asegurar que el mensaje termina en NULL.*/
                       buffer[cc] = '\0';
                       serverUDP(s_UDP, buffer, clientaddr_in);
                    }
                }
            }  /*Fin del bucle infinito de atención a clientes*/
            /*Cerramos los sockets de UDP y TCP*/
            close(ls_TCP);
            close(s_UDP);
            printf("\nFin de programa servidor!\n");

        default:  /*El padre*/
            exit(0);
   }
}



void serverTCP(int s, struct sockaddr_in clientaddr_in){

}


/*Rutina que aborta al proceso child que atiende al cliente*/
void errout(char *hostname){
    printf("Connection with %s aborted on error\n", hostname);
    exit(1);
}


void serverUDP(int s, char * buffer, struct sockaddr_in clientaddr_in){
    /*Nuevo socket para los clientes*/
    struct sockaddr_in nuevoClientaddr_in;
    socklen_t addrlen;  /*Unsigned int.*/

    int s_nc_UDP;  /*Descriptor para nuevo cliente*/

    char hostname[100];
    char bufferLector[BUFFERSIZE];
    char bufAux1[BUFFERSIZE];

    struct addrinfo hints, *res;
    struct sigaction alrm;

    char respuestaServidor[BUFFERSIZE];
    char *cadenaSeparador, *cadenaAux1;
    char crlf[] = "\r\n";
    char cabeceraCliente[3][200];
    char cabeceraRespuestaServidor[4][200];
    char cadenaConexion[3][30];
    int numLinea = 0;

    char pathWWW[] = "www";
    FILE* ficheroWeb;
    int devolver404 = 0;

    switch(fork()){
        case -1:
            printf("%s: unable to fork nc UDP\n");
            exit(1);
        case 0:
            s_nc_UDP = socket(AF_INET, SOCK_DGRAM, 0);
            if(s_nc_UDP == -1){
                printf("Unable to create nc socket UDP\n");
                exit(1);
            }
            memset(&nuevoClientaddr_in, 0, sizeof(struct sockaddr_in));

            if(bind(s_nc_UDP, (struct sockaddr *)&nuevoClientaddr_in, sizeof(struct sockaddr_in) == -1)){
                printf("Unable to bind address nc UDP\n");
                exit(1);
            }

            if(getsockname(s_nc_UDP, (struct sockaddr *)&nuevoClientaddr_in, &addrlen) == -1){
                printf("Unable to read socket adress UDP\n");
                exit(1);
            }

            getnameinfo((struct sockaddr *)&clientaddr_in, sizeof(clientaddr_in), hostname, MAXHOST,NULL,0,0);
            break;

        default:
            return;
    }

    close(s);
    sprintf(bufAux1, "Conexión en %s", hostname);
    logPeticiones(clientaddr_in.sin_addr, clientaddr_in.sin_port, "UDP", bufAux1); //Mensaje de constancia de inicio de conexión.



    addrlen = sizeof(addrlen);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;

    alrm.sa_handler = (void *) alarma;
    alrm.sa_flags = 0;
    if(sigaction(SIGALRM, &alarma, (struct sigaction *) 0) == -1){
        fprintf(stderr, "Unable to register the SIGALRM signal\n");
    }

    while(1){
        strcpy(bufAux1, "");

        cadenaSeparador = strtok(buffer, crlf);
        while(cadenaSeparador != NULL){
            strcpy(cabeceraCliente[numLinea], cadenaSeparador);
            cadenaSeparador = strtok(NULL, crlf);
            numLinea++;
        }

        numLinea = 0;
        cadenaAux1 = strtok(cabeceraCliente[numLinea], " ");
        while(cadenaAux1 != NULL){
            strcpy(cadenaSeparador[numLinea], cadenaAux1);
            cadenaAux1 = strtok(NULL, " ");
            numLinea++;
        }

        numLinea = 0;
        cadenaAux1 = strtok(cabeceraCliente[2], " ");
        while(cadenaAux1 != NULL){
            strcpy(cadenaConexion[numLinea], cadenaAux1);
            cadenaAux1 = strtok(NULL, " ");
            numLinea++;
        }

        /*Comenzamos con la respuesta del servidor, montando el mensaje desde el comienzo con HTTP.*/
        strcpy(respuestaServidor, "");
        strcpy(respuestaServidor, "HTTP/1.1 ");

        if(strcmp(cabeceraCliente[0], "GET") != 0){
            /*Error 501*/
            strcat(respuestaServidor, "501 Not Implemented");
            strcat(respuestaServidor, crlf);

            strcat(respuestaServidor, "Server: ");
            strcat(respuestaServidor, hostname);
            strcat(respuestaServidor, crlf);

            /*Conexión viva o cerrada según la tercera línea del cliente.*/
            if(strcmp(cadenaConexion[1], "keep-alive") == 0){
                strcat(respuestaServidor, "Connection: ");
                strcat(respuestaServidor, "keep-alive");
                strcat(respuestaServidor, crlf);

                strcat(respuestaServidor, crlf);
                strcat(respuestaServidor, "<html><body><h1>501 Not Implemented</h1></body></html>\n");

                //sleep(2)

                if(sendto(s_nc_UDP, respuestaServidor, BUFFERSIZE, 0, (struct sockaddr*)&clientaddr_in, addrlen) != BUFFERSIZE){
                    logPeticiones(); //Error en el reenvío.
                }

                logPeticiones(); //Mensaje de constancia de 501 devuelto.
            }else{
                strcat(respuestaServidor, "Connection: ");
                strcat(respuestaServidor, "close");
                strcat(respuestaServidor, crlf);

                strcat(respuestaServidor, crlf);
                strcat(respuestaServidor, "<html><body><h1>501 Not Implemented</h1></body></html>\n");

                //sleep(2)

                if(sendto(s_nc_UDP, respuestaServidor, BUFFERSIZE, 0, (struct sockaddr*)&clientaddr_in, addrlen) != BUFFERSIZE){
                    logPeticiones(); //Error en el envío.
                }

                logPeticiones(); //Mensaje de constancia de 501 devuelto.

                close(s_nc_UDP);
                logPeticiones(); //Fin de conexión.
                break; /*Salida del bucle infinito*/
            }
        }else{
            strcat(pathWWW, cabeceraCliente[1]);

            if((ficheroWeb = (fopen(pathWWW, "r")) == NULL)){
                devolver404 = 0;
            }else{
                devolver404 = 1;
            }

            if(!devolver404){
                /*No se encuentra el fichero especificado, 404.*/
                strcat(respuestaServidor, "404 Not found");
                strcat(respuestaServidor, crlf);

                strcat(respuestaServidor, "Server: ");
                strcat(respuestaServidor, hostname);
                strcat(respuestaServidor, crlf);

                /*Conexión viva o cerrada según la tercera línea del cliente.*/
                if(strcmp(cadenaConexion[1], "keep-alive") == 0){
                    strcat(respuestaServidor, "Connection: ");
                    strcat(respuestaServidor, "keep-alive");
                    strcat(respuestaServidor, crlf);

                    strcat(respuestaServidor, crlf);
                    strcat(respuestaServidor, "<html><body><h1>404 Not found</h1></body></html>\n");

                    //sleep(2)

                    if(sendto(s_nc_UDP, respuestaServidor, BUFFERSIZE, 0, (struct sockaddr*)&clientaddr_in, addrlen) != BUFFERSIZE){
                        logPeticiones(); //Error en el reenvío.
                    }

                    logPeticiones(); //Mensaje de constancia de 404 devuelto.
                }else{
                    strcat(respuestaServidor, "Connection: ");
                    strcat(respuestaServidor, "close");
                    strcat(respuestaServidor, crlf);

                    strcat(respuestaServidor, crlf);
                    strcat(respuestaServidor, "<html><body><h1>404 Not found</h1></body></html>\n");

                    //sleep(2)

                    if(sendto(s_nc_UDP, respuestaServidor, BUFFERSIZE, 0, (struct sockaddr*)&clientaddr_in, addrlen) != BUFFERSIZE){
                        logPeticiones(); //Error en el envío.
                    }

                    logPeticiones(); //Mensaje de constancia de 404 devuelto.

                    close(s_nc_UDP);
                    logPeticiones(); //Fin de conexión.
                    break; /*Salida del bucle infinito*/
                }
            }else{
                /*El único correcto, 200.*/
                strcat(respuestaServidor, "200 OK");
                strcat(respuestaServidor, crlf);

                strcat(respuestaServidor, "Server: ");
                strcat(respuestaServidor, hostname);
                strcat(respuestaServidor, crlf);

                if(strcmp(cadenaConexion[1], "keep-alive") == 0){
                    strcat(respuestaServidor, "Connection: ");
                    strcat(respuestaServidor, "keep-alive");
                    strcat(respuestaServidor, crlf);

                    strcat(respuestaServidor, crlf);

                    while(fgets(bufferLector, BUFFERSIZE, ficheroWeb) != NULL){
                        strcat(respuestaServidor, bufferLector);
                    }
                    fclose(ficheroWeb);

                    //sleep(2)

                    if(sendto(s_nc_UDP, respuestaServidor, BUFFERSIZE, 0, (struct sockaddr*)&clientaddr_in, addrlen) != BUFFERSIZE){
                        logPeticiones(); //Error en el envío.
                    }
                    logPeticiones();
                }else{
                    strcat(respuestaServidor, "Connection: ");
                    strcat(respuestaServidor, "close");
                    strcat(respuestaServidor, crlf);

                    strcat(respuestaServidor, crlf);

                    while(fgets(bufferLector, BUFFERSIZE, ficheroWeb) != NULL){
                        strcat(respuestaServidor, bufferLector);
                    }
                    fclose(ficheroWeb);

                    //sleep(2)

                    if(sendto(s_nc_UDP, respuestaServidor, BUFFERSIZE, 0, (struct sockaddr*)&clientaddr_in, addrlen) != BUFFERSIZE){
                        logPeticiones(); //Error en el envío.
                    }
                    close(s_nc_UDP);
                    logPeticiones();
                    break;
                }
            }
        }

    }


    //IMPLEMENTAR EL CERRADO DE UDP.


}
