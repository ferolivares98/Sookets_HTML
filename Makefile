CC = gcc
PROGS = servidor cliente

all: ${PROGS}

servidor: servidor.c utils.o utils.h
	${CC} -g servidor.c utils.o -o servidor

cliente: cliente.c utils.o utils.h
	${CC} -g cliente.c utils.o -o cliente

utils.o: utils.c utils.h
	${CC} -g -c utils.c

clean:
	rm *.o ${PROGS}
