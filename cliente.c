/*

** Fichero: cliente.c
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

extern int errno;

#define PUERTO 26671
#define BUFFERSIZE 1024
#define TIMEOUT 6
#define RETRIES	5		/* number of times to retry before givin up */

void clientTCP(int, char **);
char clientTCPOneByOne(int argc, char *argv[], char * lF, int, char);
void clientUDP(int, char **);
void clientUDPOneByOne(int argc, char *argv[], char * lF);
void handler(){
    printf("Alarma recibida");
}

int main(int argc, char *argv[]){
    FILE *fOrdenes;

    if(argc != 4 || (strcmp(argv[2], "TCP") == 1 && strcmp(argv[2], "UDP") == 1)){
        /*Aquí no hacemos la comprobación de la k y la c porque las ordenes están
        **en la carpeta con los ficheros de ordenes y por línea de comandos
        **sólo vamos a lanzar el cliente y el servidor, la comprobación de k y c
        **las haremos más adelante dentro del propio código.*/
        fprintf(stderr, "Uso correcto del programa: ");
        fprintf(stderr, "./cliente url_servidor TCP/UDP ficheroDeOrdenes.txt\n");
        exit(1);
    }

    /*Se comprueba antes que nada la capacidad de lectura (y existencia) del fichero html.*/
    if((fOrdenes = fopen(argv[3], "r")) == NULL){
        fprintf(stderr, "Error al abrir el fichero de ordenes especificado.\n");
        exit(1);
    }
    fclose(fOrdenes);

    /*Comprobar si el protocolo serA TCP o UDP*/
    if(strcmp(argv[2], "TCP") == 0){
        clientTCP(argc, argv);
    }else{
        clientUDP(argc, argv);
    }
    return 0;
}

void clientTCP(int argc, char *argv[]){
    FILE *fOrdenes;
    char lectorFichero[BUFFERSIZE];
    int contadorLinea = 0;
    char ultimoCaracter = 'c';

    if((fOrdenes = fopen(argv[3], "r")) == NULL){
        fprintf(stderr, "Error al abrir el fichero de ordenes especificado.\n");
        exit(1);
    }

    while(fgets(lectorFichero, BUFFERSIZE, fOrdenes) != NULL){
        ultimoCaracter = clientTCPOneByOne(argc, argv, lectorFichero, contadorLinea, ultimoCaracter);
        contadorLinea++;
    }
    fclose(fOrdenes);
}

char clientTCPOneByOne(int argc, char *argv[], char * lF, int contadorLinea, char ultimoCaracter){
    int s_TCP;
    struct addrinfo hints, *res;
    struct sockaddr_in myaddr_in;
    struct sockaddr_in servaddr_in;
    socklen_t addrlen;

    int errcode, len;
    int numPalabra;

    char mensajeCliente[BUFFERSIZE];
    char *mensajeClienteCortado;
    char orden[3][120];  /*Equivalente al cabRuta del servidor, mAs o menos*/
    char bufferServidorRespuesta[BUFFERSIZE];
    char respuestaServidor[BUFFERSIZE];
    char crlf[] = "\r\n";

    if(contadorLinea == 0 || ultimoCaracter == 'c'){
        s_TCP = socket (AF_INET, SOCK_STREAM, 0);
        /* Comprobación de errores de la creación del socket */
        if (s_TCP == -1) {
            perror(argv[0]);
            fprintf(stderr, "%s: unable to create socket\n", argv[0]);
            exit(1);
        }

        /* Borramos / formateamos las estructuras con las direcciones */
        memset ((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
        memset ((char *)&servaddr_in, 0, sizeof(struct sockaddr_in));

        /*Peer address al que nos vamos a conectar.*/
        servaddr_in.sin_family = AF_INET;

        /*Información del host.*/
        memset (&hints, 0, sizeof (hints));
        hints.ai_family = AF_INET;

        /*esta funciOn es la recomendada para la compatibilidad con IPv6 gethostbyname queda obsoleta.
        **Obtiene la ip asociada al nombre que le pasamos como primer argumento*/
        errcode = getaddrinfo (argv[1], NULL, &hints, &res);
        if(errcode != 0){
            /*No encuentra el nombre. Hace return indicando el error*/
            fprintf(stderr, "%s: No es posible resolver la IP de %s\n", argv[0], argv[1]);
            exit(1);
        }else{
            servaddr_in.sin_addr = ((struct sockaddr_in *) res->ai_addr)->sin_addr;
        }
        freeaddrinfo(res);

        /* Numero de puerto del servidor en orden de red*/
        servaddr_in.sin_port = htons(PUERTO);
        /*Intenta conectarse al servidor remoto a la direccion
        **que acaba de calcular en getaddrinfo, para ello le indicamos
        **el socket que acabamos de crear 's_TCP', la direccion del servidor remoto
        **'serveraddr_in', y el tamaño de la estructura de tipo sockaddr_in.*/
        if(connect(s_TCP, (const struct sockaddr *)&servaddr_in, sizeof(struct sockaddr_in)) == -1){
            perror(argv[0]);
            fprintf(stderr, "%s: unable to connect to remote\n", argv[0]);
            exit(1);
        }

        /* Aquí ya tenemos el socket conectado con el servidor.
        **si queremos escribir el puerto efímero que nos ha dado
        **el sistema para dialogar con el servidor podemos usar la
        **función getsockname(), dado el manejador del socket 's_TCP', nos devuelve
        **la estructura de tipo sockaddr (myaddr_in) que le pasamos con
        **la ip y el puerto efímero del cliente (rellena la estructura que 
        **acabamos de comentar) para ponerla por pantalla si la necesitamos.
        **También deberemos usar un puntero 'addrlen' para que se nos devuelva la longitud
        **de la dirección que nos devuelve getsockname.*/
        addrlen = sizeof(struct sockaddr_in);
        if (getsockname(s_TCP, (struct sockaddr *)&myaddr_in, &addrlen) == -1){
            perror(argv[0]);
            fprintf(stderr, "%s: unable to read socket address\n", argv[0]);
            exit(1);
        }
    }
    
    numPalabra = 0;
    mensajeClienteCortado = strtok(lF, " ");
    /*Bucle. Fallaba de uno en uno (aunque era mAs claro.*/
    while(mensajeClienteCortado != NULL){
        strcpy(orden[numPalabra], mensajeClienteCortado);
        mensajeClienteCortado = strtok(NULL, " ");
        numPalabra++;
    }
    strcpy(mensajeCliente, orden[0]);
    strcat(mensajeCliente, " ");
    strcat(mensajeCliente, orden[1]);
    strcat(mensajeCliente, " HTTP/1.1");
    strcat(mensajeCliente, crlf);
    strcat(mensajeCliente, "Host: ");
    strcat(mensajeCliente, argv[1]);
    strcat(mensajeCliente, crlf);
    if(orden[2][0] == 'k'){
        strcat(mensajeCliente, "Connection: ");
        strcat(mensajeCliente, "keep-alive");
        strcat(mensajeCliente, crlf);
    }else{
        /*Según el PDF, debe aparecer una 'c' en los ordenes.txt
        **de ejemplo. Todo lo que no sea K se tratarA
        **como close (aunque lo suyo serIa esperar por 
        **la c o por nada). No afecta en prActicamente nada.
        **No omitimos close por simpleza y claridad.
        **Creemos que seria contraproducente para el programa
        **interrumpir ahora la ejecucion por algo que seria
        **pequeNa equivoacion en los ordenes.txt*/
        strcat(mensajeCliente, "Connection: ");
        strcat(mensajeCliente, "close");
        strcat(mensajeCliente, crlf);
    }
    strcat(mensajeCliente, crlf);

    if(send(s_TCP, mensajeCliente, strlen(mensajeCliente), 0) != strlen(mensajeCliente)){
        fprintf(stderr, "%s: Error en la funciOn send()", argv[0]);
        exit(1);
    }

    if(orden[2][0] != 'k'){
        if (shutdown(s_TCP, 1) == -1) {
            perror(argv[0]);
            fprintf(stderr, "%s: unable to shutdown socket\n", argv[0]);
            exit(1);
        }

        while((len = recv(s_TCP, bufferServidorRespuesta, BUFFERSIZE, 0)) > 0){
            bufferServidorRespuesta[len] = '\0';

            if(len == -1){
                perror(argv[0]);
                fprintf(stderr, "%s: error reading result\n", argv[0]);
                exit(1);
            }
            escribirFicheroPuEfi(bufferServidorRespuesta, myaddr_in);
        }

        return 'c';
    }else{
        return 'k';
    }
    /*TCP no funcionarA si la ultima orden no es de Close.
    **Shutdown con valor 1 indica que no se enviarA mAs (se
    **puede revisar la bibliografia del informe adjunto a la
    **practica). Se ha implementado de forma que si ahora se
    **sigue con k o c, se inicie de nuevo.*/
}


void clientUDP(int argc, char *argv[]){
    FILE *fOrdenes;
    char lectorFichero[BUFFERSIZE];

    if((fOrdenes = fopen(argv[3], "r")) == NULL){
        fprintf(stderr, "Error al abrir el fichero de ordenes especificado.\n");
        exit(1);
    }

    while(fgets(lectorFichero, BUFFERSIZE, fOrdenes) != NULL){
        clientUDPOneByOne(argc, argv, lectorFichero);
    }
    fclose(fOrdenes);
}

void clientUDPOneByOne(int argc, char *argv[], char * lF){
    int s_UDP;
    struct addrinfo hints, *res;
    struct sockaddr_in myaddr_in;
    struct sockaddr_in servaddr_in;
    socklen_t addrlen;

    struct sigaction vec;  /*Sigaction de SIGARLM*/

    int errcode;
    FILE *fOrdenes;
    int numPalabra, n_retry;

    char lectorFichero[400];
    char mensajeCliente[BUFFERSIZE];
    char *mensajeClienteCortado;
    char orden[3][120];  /*Equivalente al cabRuta del servidor, mAs o menos*/
    char bufferServidorRespuesta[BUFFERSIZE];
    char crlf[] = "\r\n";

    s_UDP = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_UDP == -1){
        perror(argv[0]);
        fprintf(stderr, "%s: unable to create socket\n", argv[0]);
        exit(1);
    }
    /* Limpiar las estructuras de direcciones.*/
    memset ((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
    memset ((char *)&servaddr_in, 0, sizeof(struct sockaddr_in));

    /*Bind socket a dirección local para respuesta del servidor.*/
    myaddr_in.sin_family = AF_INET;
    myaddr_in.sin_port = 0;
    myaddr_in.sin_addr.s_addr = INADDR_ANY;
    if (bind(s_UDP, (const struct sockaddr *)&myaddr_in, sizeof(struct sockaddr_in)) == -1) {
        perror(argv[0]);
        fprintf(stderr, "%s: unable to bind socket\n", argv[0]);
        exit(1);
    }
    addrlen = sizeof(struct sockaddr_in);
    if (getsockname(s_UDP, (struct sockaddr *)&myaddr_in, &addrlen) == -1) {
            perror(argv[0]);
            fprintf(stderr, "%s: unable to read socket address\n", argv[0]);
            exit(1);
    }

    /*DirecciOn del servidor en servaddr.*/
    servaddr_in.sin_family = AF_INET;

    /*InfOrmacion del host pasada por el usuario.*/
    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET;

    /*Esta funciOn es la recomendada para la compatibilidad con IPv6 gethostbyname queda obsoleta*/
    errcode = getaddrinfo(argv[1], NULL, &hints, &res);
    if (errcode != 0){
        /*No se encuentra el error. Se devuelve
        **un valor especial  significante de este.*/
        fprintf(stderr, "%s: No es posible resolver la IP de %s\n", argv[0], argv[1]);
        exit(1);
    }else{
        /*Copia la direcciOn del host.*/
        servaddr_in.sin_addr = ((struct sockaddr_in *) res->ai_addr)->sin_addr;
    }
    freeaddrinfo(res);

    /* puerto del servidor en orden de red*/
    servaddr_in.sin_port = htons(PUERTO);

    /* Registrar SIGALRM para no quedar bloqueados en los recvfrom */
    vec.sa_handler = (void *) handler;
    vec.sa_flags = 0;
    if ( sigaction(SIGALRM, &vec, (struct sigaction *) 0) == -1) {
        perror(" sigaction(SIGALRM)");
        fprintf(stderr,"%s: unable to register the SIGALRM signal\n", argv[0]);
        exit(1);
    }


    /*Reintentos de UDP.*/
    n_retry = RETRIES;

    /*Lo hemos cambiado de variables a array aqui.*/
    /*strcpy(orden[0], "");  GET*/
    /*strcpy(orden[1], "");  ---.html*/
    /*strcpy(orden[2], "");  'k' o 'c'*/

    numPalabra = 0;
    mensajeClienteCortado = strtok(lF, " ");
    /*Bucle. Fallaba de uno en uno (aunque era mAs claro.*/
    while(mensajeClienteCortado != NULL){
        strcpy(orden[numPalabra], mensajeClienteCortado);
        mensajeClienteCortado = strtok(NULL, " ");
        numPalabra++;
    }

    strcpy(mensajeCliente, orden[0]);
    strcat(mensajeCliente, " ");
    strcat(mensajeCliente, orden[1]);
    strcat(mensajeCliente, " HTTP/1.1");
    strcat(mensajeCliente, crlf);
    strcat(mensajeCliente, "Host: ");
    strcat(mensajeCliente, argv[1]);
    strcat(mensajeCliente, crlf);
    if(orden[2][0] == 'k'){
        /*Es UDP, no hay conexiOn, por lo que esto no "importa" como tal.
        **Aun asi, se envia con "keep-alive" pero el servidor siempre va 
        **a devolver "close" (por nuestra implementacion).*/
        strcat(mensajeCliente, "Connection: ");
        strcat(mensajeCliente, "keep-alive");
        strcat(mensajeCliente, crlf);
    }else{
        strcat(mensajeCliente, "Connection: ");
        strcat(mensajeCliente, "close");
        strcat(mensajeCliente, crlf);
    }
    strcat(mensajeCliente, crlf);

    while(n_retry > 0){
        if(sendto(s_UDP, mensajeCliente, strlen(mensajeCliente), 0, (struct sockaddr *)&servaddr_in, sizeof(struct sockaddr_in)) == -1){
            perror(argv[0]);
            fprintf(stderr, "%s: unable to send request\n", argv[0]);
            exit(1);
        }

        /*Timeout en caso de que el paquete se pierda 
        **y no quedemos bloqueados indefinidamente.*/
        alarm(TIMEOUT);
        
        /*recvfrom (bloqueante). Hemos tenido muchos problemas con accesos a memoria y
        **el error ***stack smashing detected***: <unknown> terminated que
        **creemos haber solucionado.*/
        if(recvfrom(s_UDP, bufferServidorRespuesta, BUFFERSIZE, 0, (struct sockaddr *)&servaddr_in, &addrlen) == -1){
            if(errno == EINTR){
                /*Salto de alarma y receive abortado. Se
                **procede a reintentar o finalizar*/
                printf("\nattempt %d (retries %d).\n", n_retry, RETRIES);
                n_retry--;
            }else{
                printf("Unable to get response from server.\n");
                exit(1);
            }
        }else{
            alarm(0);
            strcat(bufferServidorRespuesta, "\n\n");
            escribirFicheroPuEfi(bufferServidorRespuesta, myaddr_in);
            break;
        }
    }

    if (n_retry == 0) {
        printf("Unable to get response from");
        printf(" %s after %d attempts.\n", argv[1], RETRIES);
    }
    close(s_UDP);
}