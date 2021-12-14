/*

** Fichero: servidor.c
** Autores:
** Fernando Olivares Naranjo DNI 54126671N
** Diego Sánchez Martín DNI 44435121H
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "utils.h"

#define PUERTO 26671
#define BUFFERSIZE 1024
#define TIMEOUT 3

extern int errno;

void serverTCP(int s, struct sockaddr_in clientaddr_in);
void serverUDP(int s_nc_UDP, char * buf, struct sockaddr_in clientaddr_in);
void errout(char * hostname);

int FIN = 0;
void finalizar(){ FIN = 1; }

int main(int argc, char *argv[]){
    int s_TCP;  /*Socket de TCP*/
    int ls_TCP;  /*Socket de listen TCP*/
    int s_UDP;  /*Socket de UDP*/
    int s_nc_UDP; /*Socket de nuevo cliente de UDP*/

    int cc;

    struct sigaction sa = {.sa_handler = SIG_IGN};

    struct sockaddr_in myaddr_in;
    struct sockaddr_in clientaddr_in;
    struct sockaddr_in nuevoClientaddr_in;
    socklen_t addrlen;  /*Unsigned int.*/

    fd_set readmask;
    int numfds, s_mayor;

    char buffer[BUFFERSIZE];

    struct sigaction vec;

    ls_TCP = socket(AF_INET, SOCK_STREAM, 0);
    if(ls_TCP == -1){
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
    if(bind(ls_TCP, (const struct sockaddr *)&myaddr_in, sizeof(struct sockaddr_in)) == -1){
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
        printf("%s: unable to create socket UDP\n", argv[0]);
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
            **los hijos terminen*. Evita llamadas a wait.*/
            if(sigaction(SIGCHLD, &sa, NULL) == -1){
                perror("sigaction(SIGCHLD");
                fprintf(stderr, "%s: unable to register the SIGCHLD signal\n", argv[0]);
                exit(1);
            }

            /*Registrar SIGTERM para finalización ordenada del servidor.*/
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
                        perror("\nFinalizando el servidor. Señal recibida en select\n");
                    }
                }else{
                    /*Comprobar si el socket seleccionado es TCP*/
                    if(FD_ISSET(ls_TCP, &readmask)){
                        /*Llamada bloqueante hasta nueva conexiOn.*/
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
                    }  /*De TCP.*/
                    /*Comprobar si el socket seleccionado es el UDP.*/
                    if(FD_ISSET(s_UDP, &readmask)){
                        /*Se bloqueará hasta que llegue una nueva petición. Devolverá la dirección del cliente y
                        **un buffer con la petición. El último caracter del buffer será null.*/
                        cc = recvfrom(s_UDP, buffer, BUFFERSIZE, 0, (struct sockaddr *)&clientaddr_in, &addrlen);
                        if(cc == -1){
                            perror(argv[0]);
                            printf("%s: recvfrom error\n", argv[0]);
                            exit(1);
                        }
                        buffer[cc] = '\0';

                        switch(fork()){
                            case -1:
                                perror(argv[0]);
                                fprintf(stderr, "%s: unable to fork on UDP", argv[0]);
                                exit(1);
                            
                            case 0:  /*Socket de nuevo cliente.*/
                                s_nc_UDP = socket(AF_INET, SOCK_DGRAM, 0);
                                if(s_nc_UDP == -1){
                                    perror(argv[0]);
                                    fprintf(stderr, "%s: unable to create socket new udp\n", argv[0]);
                                    exit(1);
                                }
                                nuevoClientaddr_in.sin_family = AF_INET;
                                nuevoClientaddr_in.sin_port = 0;
                                nuevoClientaddr_in.sin_addr.s_addr = INADDR_ANY;

                                if(bind(s_nc_UDP, (struct sockaddr *)&nuevoClientaddr_in, sizeof(struct sockaddr_in)) == -1){
                                    perror(argv[0]);
                                    fprintf(stderr, "%s: unable to bind adress in new UDP", argv[0]);
                                    exit(1);
                                }
                                serverUDP(s_nc_UDP, buffer, clientaddr_in);
                                exit(0);

                            default:
                                close(s_nc_UDP);
                        }
                    }
                }
            }
            close(ls_TCP);
            close(s_UDP);
            printf("\nFin de programa servidor!\n\n");

        default:
            exit(0);
    }
}

void serverTCP(int s, struct sockaddr_in clientaddr_in){
    char buf[BUFFERSIZE];		/* This example uses BUFFERSIZE byte messages. */
    char hostname[NI_MAXHOST];		/* remote host's name string */

    int len, status;

    struct linger linger;

    char crlf[] = "\r\n";
    char respuestaServidor[BUFFERSIZE];

    char cabeceraCliente[8][50];
    char cabeceraHost[8][50];
    char cabeceraConexion[8][50];

    char trozosCabeceraCliente[8][50];
    char ordenesSolicitadas[8][200]; /*Para saber que orden incluye cada linea*/
    char ficherosSolicitados[8][200]; /*Para saber que fichero ha solicitado cada orden*/
    int inicializador;
    for(inicializador = 0; inicializador < 8; inicializador++){
        strcpy(ordenesSolicitadas[inicializador], "");
        strcpy(ficherosSolicitados[inicializador], "");
    }

    int contadorCabecera, contadorTrozos;
    char *cadenaSeparador, *cadenaAux1;
    char pathWWW[] = "www";
    char pathFichero[120];
    FILE *ficheroWeb;
    int devolver404 = 0;
    char bufferLector[BUFFERSIZE];
    char bufferLectorConcatenate[BUFFERSIZE];


    char devolverError404[] = "<html><body><h1>404 Not found</h1></body></html>\n";
    char devolverError501[] = "<html><body><h1>501 Not Implemented</h1></body></html>\n";
    int contentLength;
    char contentLengthChar[6];

    int i;

    status = getnameinfo((struct sockaddr *)&clientaddr_in,sizeof(clientaddr_in), hostname,NI_MAXHOST,NULL,0,0);
    if(status){
        if (inet_ntop(AF_INET, &(clientaddr_in.sin_addr), hostname, NI_MAXHOST) == NULL)
            perror(" inet_ntop \n");
    }

    linger.l_onoff  = 1;
    linger.l_linger = 1;
    if (setsockopt(s, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) == -1) {
        errout(hostname);
    }

    /* Go into a loop, receiving requests from the remote
     * client.  After the client has sent the last request,
     * it will do a shutdown for sending, which will cause
     * an end-of-file condition to appear on this end of the
     * connection.  After all of the client's requests have
     * been received, the next recv call will return zero
     * bytes, signalling an end-of-file condition.  This is
     * how the server will know that no more requests will
     * follow, and the loop will be exited.
     * IMPORTANTE para entender keep-alive y close!
     */

    while((len = recv(s, buf, BUFFERSIZE, 0)) > 0) {
        buf[len]='\0';
        if (len == -1) errout(hostname);/* error from recv */

        logPeticiones(clientaddr_in, "TCP", "Inicio correcto.");

        contadorCabecera = 0;
        /*Metemos cada cabecera enviada por el cliente en una Variable
        **separandolas por \r\n y despues las separamos individualmente por espacios*/
        cadenaSeparador = strtok(buf, crlf);
        while(cadenaSeparador != NULL){
            strcpy(cabeceraCliente[contadorCabecera], cadenaSeparador);
            cadenaSeparador = strtok(NULL, crlf);
            strcpy(cabeceraHost[contadorCabecera], cadenaSeparador);
            cadenaSeparador = strtok(NULL, crlf);
            strcpy(cabeceraConexion[contadorCabecera], cadenaSeparador);
            cadenaSeparador = strtok(NULL, crlf);
            contadorCabecera = contadorCabecera + 1;
        }


        for(i = 0; i < contadorCabecera; i++){
            contadorTrozos = 0;
            cadenaAux1 = strtok(cabeceraCliente[i], " ");
            while(cadenaAux1 != NULL){
                strcpy(trozosCabeceraCliente[contadorTrozos], cadenaAux1);
                if(contadorTrozos == 0){
                    strcpy(ordenesSolicitadas[i], trozosCabeceraCliente[contadorTrozos]);
                }
                if(contadorTrozos == 1){
                    strcpy(ficherosSolicitados[i], trozosCabeceraCliente[contadorTrozos]);
                }
                cadenaAux1 = strtok(NULL, " ");
                contadorTrozos = contadorTrozos + 1;
            }
        }
        
        for(i = 0; i<contadorCabecera; i++) {
            /*Comenzamos con la respuesta del servidor, montando el mensaje desde el comienzo con HTTP.*/
            strcpy(respuestaServidor, "");
            strcpy(respuestaServidor, "HTTP/1.1 ");
            if(strcmp(ordenesSolicitadas[i], "GET") != 0){
                /*Error 501*/
                strcat(respuestaServidor, "501 Not Implemented");
                strcat(respuestaServidor, crlf);
                strcat(respuestaServidor, "Server: ");
                strcat(respuestaServidor, hostname);
                strcat(respuestaServidor, crlf);
                /*Conexión viva o cerrada según la tercera línea del cliente.*/
                if(strcmp(cabeceraConexion[i], "Connection: keep-alive") == 0){
                    strcat(respuestaServidor, "Connection: ");
                    strcat(respuestaServidor, "keep-alive");
                    strcat(respuestaServidor, crlf);
                }else{
                    strcat(respuestaServidor, "Connection: ");
                    strcat(respuestaServidor, "close");
                    strcat(respuestaServidor, crlf);
                }
                strcat(respuestaServidor, "Content-Length: ");
                contentLength = strlen(devolverError501);
                sprintf(contentLengthChar, "%d", contentLength);
                strcat(respuestaServidor, contentLengthChar);
                strcat(respuestaServidor, crlf);
                strcat(respuestaServidor, crlf);
                strcat(respuestaServidor, devolverError501);
                sleep(1);
                if(send(s, respuestaServidor, BUFFERSIZE, 0) != BUFFERSIZE){
                    logPeticiones(clientaddr_in, "TCP", "Error en envío de 501 Not Implemented");
                    errout(hostname);
                }
                logPeticiones(clientaddr_in, "TCP", "Envío correcto de 501 Not Implemented");
            }else{
                strcpy(pathFichero, pathWWW);
                strcat(pathFichero, ficherosSolicitados[i]);
                if((ficheroWeb = fopen(pathFichero, "r")) == NULL){
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
                    if(strcmp(cabeceraConexion[i], "Connection: keep-alive") == 0){
                        strcat(respuestaServidor, "Connection: ");
                        strcat(respuestaServidor, "keep-alive");
                        strcat(respuestaServidor, crlf);
                    }else{
                        strcat(respuestaServidor, "Connection: ");
                        strcat(respuestaServidor, "close");
                        strcat(respuestaServidor, crlf);
                    }
                    strcat(respuestaServidor, "Content-Length: ");
                    contentLength = strlen(devolverError404);
                    sprintf(contentLengthChar, "%d", contentLength);
                    strcat(respuestaServidor, contentLengthChar);
                    strcat(respuestaServidor, crlf);
                    strcat(respuestaServidor, crlf);
                    strcat(respuestaServidor, devolverError404);
                    sleep(1);
                    if(send(s, respuestaServidor, BUFFERSIZE, 0) != BUFFERSIZE){
                        logPeticiones(clientaddr_in, "TCP", "Error en envío de 404 Not found");
                        errout(hostname);
                    }
                    logPeticiones(clientaddr_in, "TCP", "Envío correcto de 404 Not found");
                }else{
                    /*El único correcto, 200.*/
                    strcat(respuestaServidor, "200 OK");
                    strcat(respuestaServidor, crlf);
                    strcat(respuestaServidor, "Server: ");
                    strcat(respuestaServidor, hostname);
                    strcat(respuestaServidor, crlf);
                    if(strcmp(cabeceraConexion[i], "Connection: keep-alive") == 0){
                        strcat(respuestaServidor, "Connection: ");
                        strcat(respuestaServidor, "keep-alive");
                        strcat(respuestaServidor, crlf);
                    }else{
                        strcat(respuestaServidor, "Connection: ");
                        strcat(respuestaServidor, "close");
                        strcat(respuestaServidor, crlf);
                    }
                    strcpy(bufferLectorConcatenate, "");
                    while(fgets(bufferLector, BUFFERSIZE, ficheroWeb) != NULL){
                        strcat(bufferLectorConcatenate, bufferLector);
                    }
                    fclose(ficheroWeb);
                    strcat(respuestaServidor, "Content-Length: ");
                    contentLength = strlen(bufferLectorConcatenate);
                    sprintf(contentLengthChar, "%d", contentLength);
                    strcat(respuestaServidor, contentLengthChar);
                    strcat(respuestaServidor, crlf);
                    strcat(respuestaServidor, crlf);
                    strcat(respuestaServidor, bufferLectorConcatenate);
                    sleep(1);
                    if(send(s, respuestaServidor, BUFFERSIZE, 0) != BUFFERSIZE){
                        logPeticiones(clientaddr_in, "TCP", "Error en envío de 200 OK");
                        errout(hostname);
                    }
                    logPeticiones(clientaddr_in, "TCP", "Envío correcto de 200 OK");
                }
            }
        }
    }
    close(s);
    logPeticiones(clientaddr_in, "TCP", "Close socket.");
}


/*Esta rutina aborta a los procesos child que atienden al cliente.*/
void errout(char * hostname){
    printf("Connection with %s aborted on error\n", hostname);
    exit(1);
}



/*ServerUDP: */
void serverUDP(int s_nc_UDP, char * buf, struct sockaddr_in clientaddr_in){
    struct addrinfo hints, *res;
    socklen_t addrlen;

    char hostname[NI_MAXHOST];
    struct linger linger;

    char buffer[BUFFERSIZE];
    char bufferLector[BUFFERSIZE];
    char bufferLectorConcatenate[BUFFERSIZE];
    char crlf[] = "\r\n";
    char originalCliente[BUFFERSIZE];
    char respuestaServidor[BUFFERSIZE];
    
    char *cadenaSeparador, *cadenaAux1;
    char cabeceraCliente[4][150];  /*Mensaje truncado completo*/
    char cabRuta[3][120];  /*LInea de /---.html*/
    int numLinPal = 0;  /*Recorre lIneas o palabras.*/

    char devolverError501[] = "<html><body><h1>501 Not Implemented</h1></body></html>";
    char devolverError404[] = "<html><body><h1>404 Not found</h1></body></html>";
    int contentLength = 0;
    char contentLengthChar[5];

    char pathWWW[] = "www";
    char pathFichero[120];
    FILE *ficheroWeb;
    int devolver404 = 0;

    /*Copia del bufefr para trabajar.*/
    strcpy(originalCliente, buf);

    addrlen = sizeof(struct sockaddr_in);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;

    linger.l_onoff = 1;
    linger.l_linger = 1;

    /*SO_LINGER esperar a toda la info recibida.*/
    if(setsockopt(s_nc_UDP, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) == -1){
        errout(hostname);
    }

    getnameinfo((struct sockaddr *)&clientaddr_in, sizeof(clientaddr_in), hostname, NI_MAXHOST, NULL, 0, 0);
    logPeticiones(clientaddr_in, "UDP", "Inicio correcto.");

    numLinPal = 0;

    /*Cabecera completa*/
    cadenaAux1 = strtok(originalCliente, crlf);
    while(cadenaAux1 != NULL){
        strcpy(cabeceraCliente[numLinPal], cadenaAux1);
        cadenaAux1 = strtok(NULL, crlf);
        numLinPal++;
    }

    /*LInea de fichero /---.html*/
    numLinPal = 0;
    cadenaSeparador = strtok(cabeceraCliente[0], " ");
    while(cadenaSeparador != NULL){
        strcpy(cabRuta[numLinPal], cadenaSeparador);
        cadenaSeparador = strtok(NULL, " ");
        numLinPal++;
    }


    /*Comienzo del montaje de la respuesta*/
    strcpy(respuestaServidor, "HTTP/1.1 ");

    if(strcmp(cabRuta[0], "GET") != 0){
        /*Error 501*/
        strcat(respuestaServidor, "501 Not Implemented");
        strcat(respuestaServidor, crlf);
        strcat(respuestaServidor, "Server: ");
        strcat(respuestaServidor, hostname);
        strcat(respuestaServidor, crlf);
        //Es UDP, independientemente del valor de k o c, close.
        strcat(respuestaServidor, "Connection: ");
        strcat(respuestaServidor, "close");
        strcat(respuestaServidor, crlf);
        strcat(respuestaServidor, "Content-Length: ");
        contentLength = strlen(devolverError501);
        sprintf(contentLengthChar, "%d", contentLength);
        strcat(respuestaServidor, contentLengthChar);
        strcat(respuestaServidor, crlf);
        strcat(respuestaServidor, crlf);
        strcat(respuestaServidor, devolverError501);
        sleep(1);
        if(sendto(s_nc_UDP, respuestaServidor, BUFFERSIZE, 0, (struct sockaddr*)&clientaddr_in, addrlen) != BUFFERSIZE){
            logPeticiones(clientaddr_in, "UDP", "Error en envío de 501 Not Implemented");
            errout(hostname);
        }
        logPeticiones(clientaddr_in, "UDP", "Envío correcto de 501 Not Implemented");
    }else{
        strcpy(pathFichero, pathWWW);
        strcat(pathFichero, cabRuta[1]);
        if((ficheroWeb = fopen(pathFichero, "r")) == NULL){
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
            strcat(respuestaServidor, "Connection: ");
            strcat(respuestaServidor, "close");
            strcat(respuestaServidor, crlf);
            strcat(respuestaServidor, "Content-Length: ");
            contentLength = strlen(devolverError404);
            sprintf(contentLengthChar, "%d", contentLength);
            strcat(respuestaServidor, contentLengthChar);
            strcat(respuestaServidor, crlf);
            strcat(respuestaServidor, crlf);
            strcat(respuestaServidor, devolverError404);
            sleep(1);
            if(sendto(s_nc_UDP, respuestaServidor, BUFFERSIZE, 0, (struct sockaddr*)&clientaddr_in, addrlen) != BUFFERSIZE){
                logPeticiones(clientaddr_in, "UDP", "Error en envío de 404 Not found");
                errout(hostname);
            }
            logPeticiones(clientaddr_in, "UDP", "Envío correcto de 404 Not found");
        }else{
            strcat(respuestaServidor, "200 OK");
            strcat(respuestaServidor, crlf);
            strcat(respuestaServidor, "Server: ");
            strcat(respuestaServidor, hostname);
            strcat(respuestaServidor, crlf);
            strcat(respuestaServidor, "Connection: ");
            strcat(respuestaServidor, "close");
            strcat(respuestaServidor, crlf);
            while(fgets(bufferLector, BUFFERSIZE, ficheroWeb) != NULL){
                strcat(bufferLectorConcatenate, bufferLector);
            }
            fclose(ficheroWeb);
            strcat(respuestaServidor, "Content-Length: ");
            contentLength = strlen(bufferLectorConcatenate);
            sprintf(contentLengthChar, "%d", contentLength);
            strcat(respuestaServidor, contentLengthChar);
            strcat(respuestaServidor, crlf);
            strcat(respuestaServidor, crlf);
            strcat(respuestaServidor, bufferLectorConcatenate);
            sleep(1);
            if(sendto(s_nc_UDP, respuestaServidor, BUFFERSIZE, 0, (struct sockaddr*)&clientaddr_in, sizeof(clientaddr_in)) != BUFFERSIZE){
                logPeticiones(clientaddr_in, "UDP", "Error en envío de 200 OK");
                errout(hostname);
            }
            logPeticiones(clientaddr_in, "UDP", "Envío correcto de 200 OK");
        }
    }

    /*TendrIamos que buscar la forma de recvfrom y sendto
    **de enviar las cadenas de mayor tamaNo.*/
    logPeticiones(clientaddr_in, "UDP", "Close socket.");
    close(s_nc_UDP);
}