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

#define PUERTO 5121
#define ADDRNOTFOUND	0xffffffff	/*Dirección de retorno de host no encontrado.*/
#define BUFFERSIZE 1024	/*Tamaño máximo de los paquetes recibidos.*/
#define MAXHOST 128

void serverTCP(int s, struct sockaddr_in peeraddr_in);
void serverUDP(int s, char * buffer, struct sockaddr_in clientaddr_in);
void errout(char *);

int FIN = 0;
void finalizar(){ FIN =1; }


int main(int argc, char *argv[]){

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
                }
                else{
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
  int reqcnt = 0;		/* keeps count of number of requests */
  char buf[BUFFERSIZE];		/* This example uses BUFFERSIZE byte messages. */
  char hostname[MAXHOST];		/* remote host's name string */

  int len, len1, status;
    struct hostent *hp;		/* pointer to host info for remote host */
    long timevar;			/* contains time returned by time() */

    struct linger linger;		/* allow a lingering, graceful close; */
                        /* used when setting SO_LINGER */

  /* Look up the host information for the remote host
   * that we have connected with.  Its internet address
   * was returned by the accept call, in the main
   * daemon loop above.
   */

     status = getnameinfo((struct sockaddr *)&clientaddr_in,sizeof(clientaddr_in),
                           hostname,MAXHOST,NULL,0,0);
     if(status){
            /* The information is unavailable for the remote
       * host.  Just format its internet address to be
       * printed out in the logging information.  The
       * address will be shown in "internet dot format".
       */
       /* inet_ntop para interoperatividad con IPv6 */
            if (inet_ntop(AF_INET, &(clientaddr_in.sin_addr), hostname, MAXHOST) == NULL)
              perror(" inet_ntop \n");
             }
    /* Log a startup message. */
    time (&timevar);
    /* The port number must be converted first to host byte
     * order before printing.  On most hosts, this is not
     * necessary, but the ntohs() call is included here so
     * that this program could easily be ported to a host
     * that does require it.
     */
     printf("Startup from %s port %u at %s", hostname, ntohs(clientaddr_in.sin_port), (char *) ctime(&timevar));

    /* Set the socket for a lingering, graceful close.
     * This will cause a final close of this socket to wait until all of the
     * data sent on it has been received by the remote host.
     */
    linger.l_onoff  =1;
    linger.l_linger =1;
    if (setsockopt(s, SOL_SOCKET, SO_LINGER, &linger,
            sizeof(linger)) == -1) {
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
     */
  while (len = recv(s, buf, BUFFERSIZE, 0)) {
    if (len == -1) errout(hostname); /* error from recv */
      /* The reason this while loop exists is that there
       * is a remote possibility of the above recv returning
       * less than BUFFERSIZE bytes.  This is because a recv returns
       * as soon as there is some data, and will not wait for
       * all of the requested data to arrive.  Since BUFFERSIZE bytes
       * is relatively small compared to the allowed TCP
       * packet sizes, a partial receive is unlikely.  If
       * this example had used 2048 bytes requests instead,
       * a partial receive would be far more likely.
       * This loop will keep receiving until all BUFFERSIZE bytes
       * have been received, thus guaranteeing that the
       * next recv at the top of the loop will start at
       * the begining of the next request.
       */
    printf("%s ", buf);
    }


      /* Increment the request count. */
    reqcnt++;
      /* This sleep simulates the processing of the
       * request that a real server might do.
       */



    sleep(1);




      /* Send a response back to the client. */
    if (send(s, buf, BUFFERSIZE, 0) != BUFFERSIZE) errout(hostname);


    /* The loop has terminated, because there are no
     * more requests to be serviced.  As mentioned above,
     * this close will block until all of the sent replies
     * have been received by the remote host.  The reason
     * for lingering on the close is so that the server will
     * have a better idea of when the remote has picked up
     * all of the data.  This will allow the start and finish
     * times printed in the log file to reflect more accurately
     * the length of time this connection was used.
     */
  close(s);

    /* Log a finishing message. */
  time (&timevar);
    /* The port number must be converted first to host byte
     * order before printing.  On most hosts, this is not
     * necessary, but the ntohs() call is included here so
     * that this program could easily be ported to a host
     * that does require it.
     */
  printf("Completed %s port %u, %d requests, at %s\n",
    hostname, ntohs(clientaddr_in.sin_port), reqcnt, (char *) ctime(&timevar));
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
                printf("");
            }
    }

}
