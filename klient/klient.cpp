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
#include <errno.h>



bool readdata(int sock, void *buf, int buflen)
{
    unsigned char *pbuf = (unsigned char *) buf;
    while (buflen > 0)
    {
        int num = recv(sock, pbuf, buflen, 0);
        if (num == -1)
        {
            if (errno == EWOULDBLOCK)
            {
                // optional: use select() to check for timeout to fail the read
                continue;
            }
            return false;
        }
        if (num == 0)
            return false;

        pbuf += num;
        buflen -= num;
    }

    return true;
}

bool readlong(int sock, long *value)
{
    if (!readdata(sock, value, sizeof(value)))
        return false;
    *value = ntohl(*value);
    return true;
}

bool readfile(int sock, FILE *f)
{
    long filesize;
    if (!readlong(sock, &filesize))
        return false;
    int progress = 0;
    int maxsize = filesize;
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
            progress += num;
            printf("%d / %d\n", progress, maxsize);
        }
        while (filesize > 0);
    }
    return true;
}
int main(int argc, char* argv[]){

    if(argc < 3){
        printf("Not enough arguments\n");
        return 1;
    }

    FILE *filehandle = fopen(argv[3], "wb");

    int sfd;
    struct sockaddr_in saddr;
    struct hostent* addrent;

    addrent = gethostbyname(argv[1]);
    sfd = socket(PF_INET, SOCK_STREAM, 0);
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(atoi(argv[2]));
    memcpy(&saddr.sin_addr.s_addr, addrent->h_addr, addrent->h_length);

    if(connect(sfd, (struct sockaddr*) &saddr, sizeof(saddr)) == -1){
        perror("Failed to connect");
        return 1;
    }
    printf("Connected to %s:%s\n", argv[1], argv[2]);


    if (filehandle != NULL)
    {
        bool ok = readfile(sfd, filehandle);
        fclose(filehandle);

        if (ok)
        {
          printf("File saved as %s\n", argv[3]);
        }
        else
            remove("imagefile.jpg");
    }
    close(sfd);

    return 0;
}
