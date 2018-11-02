#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>



bool readdata(int sock, void *buf, int buflen)
{
    unsigned char *pbuf = (unsigned char *) buf;
    int progress = 0;
    while (buflen > 0)
    {
        printf("pre recv\n");
        int num = recv(sock, pbuf, buflen, 0);
        printf("after recv\n");
       /* if (num == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAEWOULDBLOCK)
            {
                // optional: use select() to check for timeout to fail the read
                continue;
            }
            return false;
        }*/
        if (num == 0)
            return false;

        pbuf += num;
        progress += num;
        printf("%d\n", progress);
        buflen -= num;
    }

    return true;
}

bool readlong(int sock, long *value)
{
    if (!readdata(sock, value, sizeof(value)))
        return false;
    *value = ntohl(*value);
    printf("readlong\n");
    return true;
}

bool readfile(int sock, FILE *f)
{
    long filesize;
    printf("readfile\n");
    if (!readlong(sock, &filesize))
        return false;
    printf("past readlong\n");
    if (filesize > 0)
    {
        char buffer[1024];
        do
        {
            int num = std::min((size_t)filesize, sizeof(buffer));
            if (!readdata(sock, buffer, num))
                return false;
            int offset = 0;
            do
            {
                size_t written = fwrite(&buffer[offset], 1, num-offset, f);
                if (written < 1)
                    return false;
                offset += written;
            }
            while (offset < num);
            filesize -= num;
        }
        while (filesize > 0);
    }
    return true;
}
int main(int argc, char* argv[]){
    FILE *filehandle = fopen(argv[3], "wb");

    printf("ABSD\n");
    int sfd;
    struct sockaddr_in saddr;
    struct hostent* addrent;

    addrent = gethostbyname(argv[1]);
    sfd = socket(PF_INET, SOCK_STREAM, 0);
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(atoi(argv[2]));
    memcpy(&saddr.sin_addr.s_addr, addrent->h_addr, addrent->h_length);
    connect(sfd, (struct sockaddr*) &saddr, sizeof(saddr));
    printf("Connected\n");



    if (filehandle != NULL)
    {
        bool ok = readfile(sfd, filehandle);
        fclose(filehandle);

        if (ok)
        {
          printf("Zapisano plik\n");
        }
        else
            remove("imagefile.jpg");
    }
    close(sfd);

    return 0;
}
