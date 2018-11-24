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
#include <iostream>
#include <cstring>
#include <sys/stat.h>
#include <dirent.h>

using namespace std;

/**
 * Funkcja sprawdza czy plik jest plikiem zwykłym
 *
 * @param path ścieżka do pliku
 * @return wartość true lub false zależna od wyniku operacji
 */

int is_regular(const char *path)
{
    struct stat path_stat;
    lstat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

/**
* Funkcja wysyła dane przez internet
*
* @param sock deskryptor połączenia
* @param buf wskaźnik do miejsca w pamięci w którym znajdują się dane
* @param buflen ilość danych w bajtach
* @return zwraca true jeżeli sie udało lub false jeżeli nie
*/


bool senddata(int sock, void *buf, int buflen)
{
    unsigned char *pbuf = (unsigned char *) buf;

    while (buflen > 0) //póki nie wysłano wszystkiego z bufora
    {
        int num = send(sock, pbuf, buflen, 0); //spróbuj wysłać wszystko i zapisz ile udało się wysłać
        if (num == -1)
        {
            if (errno == EWOULDBLOCK)
            {
                // optional: use select() to check for timeout to fail the send
                continue;
            }
            return false;
        }

        pbuf += num; //przesuń wskaźnik o num pozycji
        buflen -= num;//zmień pozostałą ilość bajtów
    }

    return true;
}

/**
 * Wysyła liczbę używając funkcji senddata
 *
 * @param sock deskryptor połącznia
 * @param value wartość do wysłania
 * @return zwraca true jeżeli sie udało lub false jeżeli nie
 * @see senddata
 */

bool sendlong(int sock, long value)
{
    value = htonl(value);
    return senddata(sock, &value, sizeof(value));
}

/**
 * Wysyła nazwę pliku przez internet
 * analogiczna do sendfile tylko dla tablicy zapisanej w pamięci
 *
 * @param sock deskryptor połączenia
 * @param path nazwa pliku
 * @return zwraca true jeżeli sie udało lub false jeżeli nie
 * @see sendlong
 * @see senddata
 * @see sendfile
 */

bool sendpath(int sock, string path){
    long namesize = (long) path.length();
    long position = 0;
    if(!sendlong(sock, namesize)){
        return false;
    }
    if(namesize > 0){
        char buffer[1024];
        do
        {
            size_t num = std::min((size_t) namesize, sizeof(buffer));
            printf("%s\n", path.c_str());
            memcpy(buffer, path.substr(position, num).c_str(), num+1);
            senddata(sock, buffer, num);
            namesize -= num;
            position += num;
        }
        while(namesize > 0);
    }
    else
        return false;
    return true;

}

/**
 * Wysyła plik przez internet
 *
 * @param sock deskryptor połączenia
 * @param path scieżka do pliku
 * @return zwraca true jeżeli sie udało lub false jeżeli nie
 * @see senddata
 * @see sendlong
 */

bool sendfile(int sock, string path)
{
    FILE *f = fopen(path.c_str(), "rb");//otwarcie pliku do odczytu
    if(f == NULL){//jeżeli się nie udało
        perror("Failed to open file");//to wypisz czemu
        return false;//i zakończ
    }
    fseek(f, 0, SEEK_END);//znajdz koniec
    long filesize = ftell(f);//sprawdź rozmiar
    rewind(f);//wróć na początek
    if (filesize == EOF)//jeżeli rozmiar = -1
        return false;// to zakończ z błędem
    if (!sendlong(sock, filesize))//wyślij rozmiar i jeżeli się nie udało
        return false;//zakończ z błędem
    if (filesize > 0)//jeżeli plik jest większy niż 0
    {
        char buffer[1024];//stwórz buffor
        do
        {
            size_t num = std::min((size_t)filesize, sizeof(buffer));//wybierz mniejsze rozmiar buffor czy pozostały rozmiar pliku do wysłania
            num = fread(buffer, 1, num, f);//przeczytaj num znaków o wielkości 1 bajt do buffer z pliku f i przypisz ile udało się odczytać do num
            if (num < 1)
                return false;
            if (!senddata(sock, buffer, num))//wyślij num znaków z buffer
                return false;
            filesize -= num;//zmniejsz pozostały rozmiar pliku o ilość wysłanych bajtów
        }
        while (filesize > 0);//póki coś zostało do wysłania
    }
    fclose(f);//zamknij plik
    return true;
}

/**
 * Wysyła katalog przez internet
 *
 * @param sock deskryptor katalogu
 * @param path scieżka do katalogu
 * @return zwraca true jeżeli sie udało lub false jeżeli nie
 * @see sendfile
 * @see sendpath
 */

bool senddirectory(int sock, string path){
    DIR *directory = opendir(path.c_str()); //otwarcie katalogu jako strumień
    long mode = 1; //zmienna odpowiadająca za typ pliku do wysłania
    struct dirent* file = readdir(directory); //przeczytanie pierwszego pliku
    do
    {
        string name = file -> d_name; //przypisanie nazwy pliku do name
        if(!is_regular((path + "/" +  file -> d_name).c_str()) )//jeżeli katalog
        {
            if(strcmp(name.c_str(), ".") && name.length() > 1)//jeżeli nie ten sam katalog
            {
                if(strcmp(name.c_str(),"..") && name.length() > 2)//jeżeli nie katalog wcześniejszy
                {
                    mode = 1; //1 w kliencie odpowiada za odebranie katalogu
                    if(!sendlong(sock, mode)){//wysłanie trybu odczytu
                        return false;
                    }
                    sendpath(sock, name); //wysłanie nazwy katalogu
                    senddirectory(sock, path + "/" + name);//otwarcie katalogu aby można było go wysłać
                }
            }
        }
        else{//jeżeli plik
            mode = 2;//odpowiada za czytanie pliku w kliencie
            sendlong(sock, mode);//wyslanie trybu
            sendpath(sock, name);//wyslanie nazwy pliku
            sendfile(sock, (path + "/" + name));//wysłanie pliku
        }
        file = readdir(directory);//odczytanie następnego pliku w katalogu
    }while(file);//tak długo jak pliku w katalogu
    sendlong(sock, 0);//wyslanie trybu zakończenia katalogu
    return true;
}

/**
* Funkcja odczytuje dane z internetu
*
* @param sock deskryptor połączenia
* @param buf wskaźnik do miejsca w pamięci w którym zapisane zostaną dane
* @param buflen ilość danych w bajtach
* @return zwraca true jeżeli sie udało lub false jeżeli nie
*/
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

/**
 * odczytuje zmienna typu long z internetu
 * @param sock deskryptor połączenia
 * @param value wskaźnik na zmieną do której zapisana zostanie wartość
 * @return zwraca true jeżeli sie udało lub false jeżeli nie
 */

bool readlong(int sock, long *value)
{
    if (!readdata(sock, value, sizeof(value)))
        return false;
    *value = ntohl(*value);
    return true;
}

/**
 * odczytuję nazwę pliku
 * @param sock deskryptor połączenia
 * @return zwraca (string) z nazwą
 * @see readlong
 */

string readpath(int sock)
{
    long namesize = 0;
    string path = "";
    if (!readlong(sock, &namesize)){//odczytanie długości nazwy
        return NULL;
    }
    if(namesize > 0){//jeżeli długość jest większa niż 0
        char buffer[1024];
        do
        {
            size_t num = std::min((size_t) namesize, sizeof(buffer));//wybierz mniejsze rozmiar nazwy lub bufora
            if(!readdata(sock, buffer, num))//odczytaj dane
                return NULL;
            for (unsigned int i = 0; i<num ;i++)//przepisz buffor do pliku znak po znaku
                path += buffer[i];
            namesize -= num; //zmniejsz pozostały rozmiar nazwy pliku o num
        }
        while(namesize > 0);//do czasu aż rozmiar nazwy pliku
    }
    else
        return NULL;
    return path;

}

/**
 * odczytuje plik z internetu
 * @param sock deskryptor połączenia
 * @param f desktryptor pliku
 * @return zwraca true jeżeli sie udało lub false jeżeli nie
 * @see readlong
 */

bool readfile(int sock, FILE *f)
{
    long filesize = 0;
    if (!readlong(sock, &filesize)) //odczytaj rozmiar pliku
        return false;
    int progress = 0;
    int maxsize = filesize;
    if (filesize > 0)
    {
        char buffer[1024];
        do
        {
            size_t num = std::min((size_t)filesize, sizeof(buffer));
            if (!readdata(sock, buffer, num)) //odczytaj num danych
                return false;
            size_t offset = 0;
            do
            {
                size_t written = fwrite(&buffer[offset], 1, num-offset, f); //przepisz je do pliku
                if (written < 1)
                    return false;
                offset += written; //zmień pozycję kursora w pamięci
            }
            while (offset < num); //powtarzaj dopóki nie przepiszesz wszystkiego co odebrałeś
            filesize -= num;//zmienjsz pozostały rozmiar pliku
            progress += num;
            printf("%d / %d\n", progress, maxsize); //wypisywanie ile udało się odebrać
        }
        while (filesize > 0);
    }
    return true;
}

/**
 * odbiera strukturę katalogów wraz z plikami
 * @param sock desktryptor połączenia
 * @param path scieżka gdzie odebrać "." dla aktualnego katalogu
 * @return zwraca true jeżeli sie udało lub false jeżeli nie
 */

bool recvdirectory(int sock, string path){
    FILE *filehandle;
    long mode = 0;
    string name;
    do
    {
        if(!readlong(sock, &mode)){ //czytanie trybu
            return false;
        }
        if(mode != 0)
        {
            name = readpath(sock);//odczytaj nazwę pliku/katalogu
        }
        if (mode == 1){//odbierz katalog
            mkdir((path + "/" + name).c_str(), 0777); //stwórz katalog
            recvdirectory(sock, (path + "/" + name)); //wejdz do katalogu aby móc go odczytać
        }
        else if(mode == 2){//odbierz plik
            filehandle = fopen((path + "/" + name).c_str(), "wb");
            readfile(sock, filehandle);
            fclose(filehandle);

        }
    }
    while(mode);
    return true;
}

int main(int argc, char* argv[]){

    if(argc < 2){
        printf("Not enough arguments\n");
        return 1;
    }


    int sfd;
    struct sockaddr_in saddr;
    struct hostent* addrent;
    bool ok;

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
    string path = "Odebrane_pliki";
    mkdir(path.c_str(),0777);
    if(!is_regular("/home/mariusz/Pulpit/minecraft"))//jeżeli katalog
    {
        if(senddirectory(sfd, "/home/mariusz/Pulpit/minecraft"))
        {
            printf("Wyslano pliki bez problemów\n");
        }

    }
    else
    {
        sendfile(sfd, "nicki.jpeg");
    }

    ok = recvdirectory(sfd, path);
    if (ok)
    {
        printf("Udalo sie odczytac pliki\n");
    }
    else
        remove("imagefile.jpg");
    close(sfd);
    return 0;
}
