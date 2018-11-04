#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/param.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <algorithm>
#include <errno.h>

bool senddata(int sock, void *buf, int buflen)
{
    unsigned char *pbuf = (unsigned char *) buf;

    while (buflen > 0)
    {
        int num = send(sock, pbuf, buflen, 0);
        if (num == -1)
        {
            if (errno == EWOULDBLOCK)
            {
                // optional: use select() to check for timeout to fail the send
                continue;
            }
            return false;
        }

        pbuf += num;
        buflen -= num;
    }

    return true;
}

bool sendlong(int sock, long value)
{
    value = htonl(value);
    return senddata(sock, &value, sizeof(value));
}

bool sendfile(int sock, FILE *f)
{
    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    rewind(f);
    if (filesize == EOF)
        return false;
    if (!sendlong(sock, filesize))
        return false;
    if (filesize > 0)
    {
        char buffer[1024];
        do
        {
            size_t num = std::min((size_t)filesize, sizeof(buffer));
            num = fread(buffer, 1, num, f);
            if (num < 1)
                return false;
            if (!senddata(sock, buffer, num))
                return false;
            filesize -= num;
        }
        while (filesize > 0);
    }
    return true;
}
int main(int argc, char** argv)
{
    struct sockaddr_in srv;
    struct sockaddr_in client;
    unsigned int size;
    int nfd, on = 1;
    FILE *filehandle = fopen("nicki.jpeg", "rb");
    if(filehandle == NULL){
        perror("Failed to open file");
        return 1;
    }

    srv.sin_family = AF_INET;
    srv.sin_port = htons(1234);
    srv.sin_addr.s_addr = INADDR_ANY;

    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if(fd == -1){
        perror("Failed to create socket");
        return 1;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*) &on, sizeof(on));

    if(bind(fd, (struct sockaddr*)&srv, sizeof(srv)) == -1){
        perror("Failed to bind");
        return 1;
    }
    if(listen(fd, 5) == -1){
        perror("Failed to listen");
        return 1;
    }

    while(1)
    {
        size = sizeof(client);
        nfd = accept(fd, (struct sockaddr*)&client, &size); //zwraca nowy dyskryptor
        if(fork() == 0)
        {
                close(fd);
                printf("new connection %s:%d\n",inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                if (filehandle != NULL)
                {
                    sendfile(nfd, filehandle);
                }
                close(nfd);
                exit(EXIT_SUCCESS);
        }
    }
    close(fd);
    fclose(filehandle);
}
