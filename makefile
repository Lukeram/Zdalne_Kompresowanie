.PHONY: all
all: pakowaczklient pakowaczserver

pakowaczklient: klient/klient.cpp
	g++ -Wall -pedantic -O3 -o klient/klient.out klient/klient.cpp

pakowaczserver: server/server.cpp
	g++ -Wall -pedantic -O3 -o server/server.out server/server.cpp

.PHONY: clean
clean:
	rm -f klient/klient.out server/server.out
