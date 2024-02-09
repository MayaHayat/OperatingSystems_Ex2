all: client 

client : client.o
	gcc -o client client.o -lssl -lcrypto


client.o: client.c
	gcc -c client.c

clean:
	rm -f *.o client 