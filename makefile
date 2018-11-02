.PHONY: all
all: pakowaczklient pakowaczserver

pakowaczklient: klient/klient.cpp
	g++ -Wall -pedantic -O3 -o klient/klient.out klient/klient.cpp

pakowaczserver: server/server.cpp
	g++ -Wall -pedantic -O3 -o server/server.out server/server.cpp

.PHONY: imagefile.jpg
imagefile.jpg: pakowaczklient
	./pakowaczklient debian-mariusz 1234

.PHONY: test
test: imagefile.jpg
	xdg-open imagefile.jpg

.PHONY: serve
serve: pakowaczserver
	./pakowaczserver ./marian.tar.gz

.PHONY: clean
clean:
	rm -f klient/klient.out server/server.out klient/imagefile.jpg
