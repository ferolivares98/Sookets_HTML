/*

** Fichero: cliente.c
** Autores:
** Fernando Olivares Naranjo DNI 54126671N
** Diego Sánchez Martín DNI 54126671N
*/


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <time.h>

#define PUERTO 5121
#define TAMANOBUFFER 1024

int main(int argc, char *argv[])
{
    int s;				/* Variable con la que crearemos el socket */
   	struct addrinfo hints, *res;
    long timevar;			/* contains time returned by time() */
    struct sockaddr_in myaddr_in;	/* Para la dirección local del socket */
    struct sockaddr_in servaddr_in;	/* Para iniciar los datos del servidor al que nos conectaremos */
	  int addrlen, i, j, errcode;
    /* This example uses TAMANOBUFFER byte messages. */
	  char buffer[TAMANOBUFFER];
    FILE *ficheroDeOrdenes;

  /* Comprobación de parámetros introduccidos por el usuario */
	if (argc != 4 || (strcmp(argv[2], "UDP") == 1 && strcmp(argv[2], "TCP") == 1))
  {
      //Aquí no hacemos la comprobación de la k porque las ordenes están
      //en la carpeta con los ficheros de ordenes y por línea de comandos
      //sólo vamos a lanzar el cliente y el servidor, la comprobación de la k
      //la haremos más adelante dentro del propio código.
    	fprintf(stderr, "Uso correcto del programa: \n");
      fprintf(stderr, "%s nombreServidor TCP/UDP\n", argv[0]);
    	exit(1);
	}

  if ((ficheroDeOrdenes = (fopen(argv[3], "r"))) == NULL)
  {
    fprintf(stderr, "Error al abrir el fichero de ordenes especificado", argv[4]);
    exit(1);
  }

	/* El cliente crea el socket de tipo SOCK_STREAM por ser TCP */
	s = socket (AF_INET, SOCK_STREAM, 0);
  /* Comprobación de errores de la creación del socket */
	if (s == -1) {
		perror(argv[0]);
		fprintf(stderr, "%s: unable to create socket\n", argv[0]);
		exit(1);
	}

	/* Borramos / formateamos las estructuras con las direcciones */
	memset ((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
	memset ((char *)&servaddr_in, 0, sizeof(struct sockaddr_in));

	/* Set up the peer address to which we will connect. */
	servaddr_in.sin_family = AF_INET;

	/* Get the host information for the hostname that the
	 * user passed in. */
      memset (&hints, 0, sizeof (hints));
      hints.ai_family = AF_INET;
 	 /* esta funci�n es la recomendada para la compatibilidad con IPv6 gethostbyname queda obsoleta*/
    errcode = getaddrinfo (argv[1], NULL, &hints, &res); //Obtiene la ip asociada al nombre que le pasamos como primer argumento
    if (errcode != 0){
			/* No encuentra el nombre. Hace return indicando el error */
		    fprintf(stderr, "%s: No es posible resolver la IP de %s\n", argv[0], argv[1]);
		    exit(1);
    } else {
		/* Copia la dirección del host */
		    servaddr_in.sin_addr = ((struct sockaddr_in *) res->ai_addr)->sin_addr;
	  }
    freeaddrinfo(res);

    /* Numero de puerto del servidor en orden de red*/
	   servaddr_in.sin_port = htons(PUERTO);

		/* Intenta conectarse al servidor remoto a la direccion
     * que acaba de calcular en getaddrinfo, para ello le indicamos
     * el socket que acabamos de crear 's', la direccion del servidor remoto
     * 'serveraddr_in', y el tamaño de la estructura de tipo sockaddr_in
		 */
	if (connect(s, (const struct sockaddr *)&servaddr_in, sizeof(struct sockaddr_in)) == -1) {
		perror(argv[0]);
		fprintf(stderr, "%s: unable to connect to remote\n", argv[0]);
		exit(1); /*Comprobacion de errores del connect */
	}
		/* Aquí ya tenemos el socket conectado con el servidor,
     * si queremos escribir el puerto efímero que me ha dado
     * el sistema para dialogar con el servidor podemos usar la
     * función getsockname(), dado el manejador del socket 's', me devuelve
     * la estructura de tipo sockaddr (myaddr_in) que le pasamos con
     * la ip y el puerto efímero del cliente (rellena la estrucura que acabamos de comentar)
     * Para ponerla por pantalla si la necesitamos.
     * También deberemos usar un puntero 'addrlen' para que se nos devuelva la longitud
     * de la dirección que nos devuelve getsockname.
		 */
	addrlen = sizeof(struct sockaddr_in);
	if (getsockname(s, (struct sockaddr *)&myaddr_in, &addrlen) == -1) {
		perror(argv[0]);
		fprintf(stderr, "%s: unable to read socket address\n", argv[0]);
		exit(1);
	 }

	/* Print out a startup message for the user. */
	time(&timevar);
	/* The port number must be converted first to host byte
	 * order before printing.  On most hosts, this is not
	 * necessary, but the ntohs() call is included here so
	 * that this program could easily be ported to a host
	 * that does require it.
	 */
	printf("Connected to %s on port %u at %s", argv[1], ntohs(myaddr_in.sin_port), (char *) ctime(&timevar));

  /* Una vez conectados usaremos el handler/manejador del socket que hemos creado
  * 's' para dialogar con el servidor mediante las funciones "send()" y "recv()"
  */

	/* Empezamos la parte de la práctica de 2021 HTTPS
  * leemos el fichero de ordenes hasta que la funcion
  * fgets() nos devuelva null */

  char ordenCliente[3]; //Cada línea del fichero de ordenes separada en trozos
  char opcion[10], html[50], opcionK[1]; //Cada uno de los trozos de cada línea del fichero de órdenes
  char *linea; //Cada línea del fichero de órdenes
  int longitudLinea; //Para saber si la orden del fichero es correcta

  while(fgets(buffer, TAMANOBUFFER, ficheroDeOrdenes) != NULL)
  {
    //Con esto conseguimos separar cada línea del fichero de ordenes en la orden, el html y la k
    linea = strtok(buffer, " ");
      if(strlen(linea) > 3)
      {
        printf("Error\n");
      }
  }

  /* Despues de realizar la función send() llamamos a shutdown() y le
  * pasamos el parámetro 1 para cerrar el socket solo de nuestro lado,
  * es decir, le indicamos al servidor que no vamos a enviar más datos
  * por este lado, pero que sí estamos abiertos para recibir información
  */

	if (shutdown(s, 1) == -1) {
		perror(argv[0]);
		fprintf(stderr, "%s: unable to shutdown socket\n", argv[0]);
		exit(1);
	}

  /* Despues de indicarle al servidor que no enviaremos más datos pero
  * que sí estamos abiertos a recibirlos, esperamos dicha información y
  * para ello usamos la función recv(), que devuelve 0 cuando se hayan enviado
  * todos los datos por parte del servidor y la conexión se haya cerrado por
  * su parte.
  */

	while (i = recv(s, buffer, TAMANOBUFFER, 0)) {
		if (i == -1) {
            perror(argv[0]);
			fprintf(stderr, "%s: error reading result\n", argv[0]);
			exit(1);
		}
			/* The reason this while loop exists is that there
			 * is a remote possibility of the above recv returning
			 * less than TAMANOBUFFER bytes.  This is because a recv returns
			 * as soon as there is some data, and will not wait for
			 * all of the requested data to arrive.  Since TAMANOBUFFER bytes
			 * is relatively small compared to the allowed TCP
			 * packet sizes, a partial receive is unlikely.  If
			 * this example had used 2048 bytes requests instead,
			 * a partial receive would be far more likely.
			 * This loop will keep receiving until all TAMANOBUFFER bytes
			 * have been received, thus guaranteeing that the
			 * next recv at the top of the loop will start at
			 * the begining of the next reply.
			 */
		while (i < TAMANOBUFFER) {
			j = recv(s, &buffer[i], TAMANOBUFFER-i, 0);
			if (j == -1) {
                     perror(argv[0]);
			         fprintf(stderr, "%s: error reading result\n", argv[0]);
			         exit(1);
               }
			i += j;
		}
			/* Print out message indicating the identity of this reply. */
		printf("Received result number %d\n", *buffer);
	}

    /* Print message indicating completion of task. */
	time(&timevar);
	printf("All done at %s", (char *)ctime(&timevar));
}

