#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>  
#include <sys/wait.h>
#include <sys/stat.h> 
#include <sys/types.h>
#include <dirent.h> 
#include <sys/socket.h>
#include <fcntl.h>
#include <time.h>


void mesajIntrare(int, char []);
void print(char []);
void citire(char [], int *, int);
void executare(char[], char[], int, int *);
void verifComanda(char [], char [], int *, int);
char* substring(char [], int, int);
int login(char []);
int cautaInFisier(char []);
void myfind(char []);
void myfindRec(char [], char [], int*, char [][1024]);
char* infoFind(char []);
void mystat(char[]);
char* infoStat(char []);
int logout();


void mesajIntrare(int logat, char user[1024])
{
    int count = 0;
    if(logat)
        printf("\nBuna, %s!\n", user);
    else
        printf("\nNu sunteti conectat.\n");
    printf("Puteti folosi urmatoarele comenzi:\n");
    if(!logat)
        printf("  %d. login : username\n", ++count);
    if(logat)
    {
        printf("  %d. myfind file\n", ++count);
        printf("  %d. mystat file\n", ++count);
        printf("  %d. logout\n", ++count);
    }
    printf("  %d. quit\n", ++count);
}

void print_(char s[1024])
{
    int count = 2 + 2 * 3 + strlen(s);
    for(int i = 0; i < count; i++)
        printf("-");
    printf("\n|   %s   |\n", s);
    for(int i = 0; i < count; i++)
        printf("-");
    printf("\n");
}

void citire(char argument[1024], int *indice, int logat)
{
    char comanda[1024];
    do{ 
        printf("Introduceti comanda:\n---> ");
        fgets(comanda, 1024, stdin); // ia cu tot cu \n
        comanda[strlen(comanda) - 1] = '\0'; // scapam de \n 
        verifComanda(comanda, argument, &*indice, logat);

        if(*indice == -1)
            print_("Comanda nu este acceptata.");
        else
        {
            print_("Comanda acceptata.");
            printf("Asteptati...\n");
            sleep(1);
        }
    }while(*indice == -1); // am primit o comanda valida
}

char* substring(char s[1024], int start, int lung)
{
    char* rez = (char*) malloc(lung);
    strncpy(rez, s + start, lung);
    rez[lung] = '\0';
    return rez;
}

void verifComanda(char comanda[1024], char argument[1024], int *indice, int logat)
/* verifica daca comanda este corecta si returneaza indicele corespunzator comenzii:
-1 -> nu e ok
1  -> login : username
2  -> myfind file
3  -> mystat file
4  -> quit 
5  -> logout */           
{
    if(!logat && strlen(comanda) > 8 && strcmp(substring(comanda, 0, 8), "login : ") == 0)      
    {
        *indice = 1;
        strcpy(argument, substring(comanda, 8, strlen(comanda) - 8));
    }
    else if(logat && strlen(comanda) > 7 && strcmp(substring(comanda, 0, 7), "myfind ") == 0)
    {
        *indice = 2;
        strcpy(argument, substring(comanda, 7, strlen(comanda) - 7));
    } 
    else if(logat && strlen(comanda) > 7 && strcmp(substring(comanda, 0, 7), "mystat ") == 0)
    {
        *indice = 3;
        strcpy(argument, substring(comanda, 7, strlen(comanda) - 7));
    }
    else if(strcmp(comanda, "quit") == 0)
        *indice = 4;
    else if(logat && strcmp(comanda, "logout") == 0)
        *indice = 5;
    else
        *indice = -1;
}

char mesaj[2048];
int lungime;
void executare(char argument[1024], char user[1024], int indice, int *logat)
{
    switch(indice)
    {
        case 1: // pipe (intern)
            if(!*logat)
            {
                *logat = login(argument);
                if(*logat)
                    strcpy(user, argument);
            }
            break;
        case 2: // socketpair 
            myfind(argument);
            break;
        case 3: // fifo (pipe extern)
            mystat(argument);
            break;
        case 4: 
            print_("La revedere! :*");
            exit(4);
        case 5:   
            *logat = logout();
            break;
        default: ;
    }
}

int login(char argument[1024]) 
{
    pid_t pid;
    int p[2];  // 0 -> citire, 1 -> scrie
    if(pipe(p) == -1)
    {
        perror("pipe()\n");
        exit(0);
    }

    switch(pid = fork())
    {
        case -1:
            perror("fork()\n");
            exit(0);

        case 0:     // prunc
            close(p[0]);

            int gasit = cautaInFisier(argument);
            if(gasit)
            {
                strcpy(mesaj, "Ati fost logat cu succes cu username-ul: ");
                strcat(mesaj, argument);
            }
            else
                strcpy(mesaj, "Username-ul nu este corect. Nu ati fost logat cu succes.");
            lungime = strlen(mesaj) + 1;  // !!! + 1(sa poata avea '\0')

            write(p[1], &lungime, sizeof(int));
            write(p[1], mesaj, lungime);
            close(p[1]);
            exit(0);

        default:    // parinte
            close(p[1]);
            
            wait(NULL); // asteptam sclavu sa caute in fisier

            read(p[0], &lungime, sizeof(int));
            read(p[0], &mesaj, lungime);
            close(p[0]);

            print_(mesaj);
            if(mesaj[0] == 'A')
                return 1;
            return 0;
    }
}

int cautaInFisier(char name[1024])
{
    FILE *f = fopen("usernames.txt", "r");
    char username[1024];
    while(fgets(username, 1024, f))
    {
        username[strlen(username) - 1] = '\0';
        if(strcmp(name, username) == 0)
        {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

void myfind(char numeFisier[1024])
{
    //mesaj, lungime;
    int sock[2]; // [0] -> copil; [1] -> parinte;
    pid_t pid;
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sock) == -1)
    {
        perror("socketpair()\n");
        exit(0);
    }

    switch(pid = fork())
    {
        case -1:
            perror("fork()\n");
            exit(0);
        
        case 0: // prunc -> sock[0]
            close(sock[1]);

            int count = 0;
            char detalii[100][1024];
            myfindRec("/home", numeFisier, &count, detalii);
            if(count == 0)
                sprintf(mesaj, "Nu exista in sistem fisierul: %s.", numeFisier);
            else
            {
                sprintf(mesaj, "Fisierul %s a fost gasit.\n\n", numeFisier);
                for(int i = 0; i < count; i++)
                {
                    char s[2048];
                    sprintf(s, "%d) %s\n", i + 1, detalii[i]);
                    strcat(mesaj, s);
                }
            }
            lungime = strlen(mesaj) + 1;
            
            write(sock[0], &lungime, sizeof(int));
            write(sock[0], mesaj, lungime);
            close(sock[0]);
            exit(0);
        
        default: // parinte -> sock[1]
            close(sock[0]);
            
            wait(NULL);

            read(sock[1], &lungime, sizeof(int));
            read(sock[1], mesaj, lungime);
            close(sock[1]);

            if(mesaj[0] == 'N')
                print_(mesaj);
            else
            {
                int i = 1;
                while(mesaj[i] != '\n')
                    i++;
                print_(substring(mesaj, 0, i));
                printf("%s", substring(mesaj, i + 1, strlen(mesaj) - i));
            }
    }
}

void myfindRec(char drumFisier[1024],char numeFisier[1024], int *count, char detalii[100][1024])
{
    struct stat myStat;
    if(stat(drumFisier, &myStat) == -1) // obtine informatii despre document si le scrie in structura
        return;
    // // st_mode -> detalii despre tipul fisierului
    if(S_ISDIR(myStat.st_mode) == 0)
        return; 
    DIR *folder = opendir(drumFisier);
    if(folder == NULL)
        return;

    struct dirent *myDirent;
    while(myDirent = readdir(folder))
    {
        char fisierCurent[1024];
        strcpy(fisierCurent, myDirent->d_name);
        if(strcmp(fisierCurent, "..") && strcmp(fisierCurent, ".")) // altfel mare eroare
        {
            char drum[1050] = "";
            strcpy(drum, drumFisier);
            strcat(drum, "/");
            strcat(drum, fisierCurent);
            if(strcmp(fisierCurent, numeFisier) == 0)     
                strcat(detalii[(*count)++], infoFind(drum));
            myfindRec(drum, numeFisier, &*count, detalii);
        }
    }
    closedir(folder);
}

char* infoFind(char numeFisier[1024])
{
    char *rez = (char*) malloc(1050);
    struct stat myStat;
    if(stat(numeFisier, &myStat) == -1)
    {
        //perror("stat()\n");
        return "";
        exit(0);
    }
    char perm[11], nr[5] = "0";

    int k = 0;
    strcat(perm, S_ISDIR(myStat.st_mode) ? "d" : "-");
    strcat(perm, myStat.st_mode & S_IRUSR ? k += 4, "r" : "-");
    strcat(perm, myStat.st_mode & S_IWUSR ? k += 2, "w" : "-");
    strcat(perm, myStat.st_mode & S_IXUSR ? k += 1, "x" : "-");
    nr[1] = '0' + k;
    k = 0;
    strcat(perm, myStat.st_mode & S_IRGRP ? k += 4, "r" : "-");
    strcat(perm, myStat.st_mode & S_IWGRP ? k += 2, "w" : "-");
    strcat(perm, myStat.st_mode & S_IXGRP ? k += 1, "x" : "-");
    nr[2] = '0' + k;
    k = 0;
    strcat(perm, myStat.st_mode & S_IROTH ? k += 4, "r" : "-");
    strcat(perm, myStat.st_mode & S_IWOTH ? k += 2, "w" : "-");
    strcat(perm, myStat.st_mode & S_IXOTH ? k += 1, "x" : "-");
    nr[3] = '0' + k;
    nr[4] = '\0';

    sprintf(rez, "Fisier:              %s \n   Dimensiune:          %ld bytes \n   Permisiuni:          %s/%s\n   Ultima accesare:     %s   Ultima modificare:   %s   Data crearii:        -----\n", 
    numeFisier, myStat.st_size, perm, nr, ctime(&myStat.st_atime), ctime(&myStat.st_mtime));// myStat.st_birthtime);
    return rez;
}

void mystat(char numeFisier[1024])
{
    pid_t pid;
    int f;
    char numeFifo[] = "mareFIFO";
    remove(numeFifo);
    if(mkfifo(numeFifo, 0666) == -1)
    {
        perror("mkfifo()\n");
        exit(0);
    }
    
    switch(pid = fork())
    {
        case -1:
            perror("fork()\n");
            exit(0);

        case 0: // prunc
            strcpy(mesaj, "\n   ");
            strcat(mesaj, infoFind(numeFisier));
            strcat(mesaj, infoStat(numeFisier));

            f = open(numeFifo, O_WRONLY);
            if(strcmp(mesaj, "\n   ") == 0)
                sprintf(mesaj, "Nu exista in sistem fisierul: %s.", numeFisier);
            lungime = strlen(mesaj) + 1;
            write(f, &lungime, sizeof(int));
            write(f, mesaj, lungime);
            close(f);

            exit(0);

        default: // parinte
            f = open(numeFifo, O_RDONLY);
            read(f, &lungime, sizeof(int));
            read(f, mesaj, lungime);
            if(mesaj[0] == 'N')
                print_(mesaj);
            else
                printf("%s", mesaj);
            close(f);
    }
    remove(numeFifo);
}

char* infoStat(char numeFisier[1024])
{
    char* rez = (char*)malloc(1024);
    struct stat myStat;
    if(stat(numeFisier, &myStat) == -1)
    {
        //perror("stat()\n");
        return "";
        exit(0);
    }
    strcpy(rez, "");
    sprintf(rez, "   Utilizator ID: %d   Grup ID: %d\n   Dimensiune bloc: %ld   Numar blocuri: %ld1\n   Numar linkuri: %ld   Inode: %ld   \n",
    myStat.st_uid, myStat.st_gid, myStat.st_blksize, myStat.st_blocks, myStat.st_nlink, myStat.st_ino);

    strcat(rez, S_ISDIR(myStat.st_mode) ? "   Fisierul este un director.\n" : "");
    strcat(rez, S_ISREG(myStat.st_mode) ? "   Fisierul este unul obsinuit.\n" : "");
    strcat(rez, S_ISSOCK(myStat.st_mode) ? "   Fisierul este un socket.\n" : "");
    strcat(rez, S_ISFIFO(myStat.st_mode) ? "   Fisierul este un FIFO.\n" : "");
    strcat(rez, S_ISLNK(myStat.st_mode) ? "   Fisierul este un link symbolic.\n" : "");
    strcat(rez, S_ISCHR(myStat.st_mode) ? "   Fisierul este un dispozitiv de caractere.\n" : "");
    strcat(rez, S_ISBLK(myStat.st_mode) ? "   Fisierul este un dispozitiv de blocuri.\n" : "");
    strcat(rez, "\n");
    return rez;
}

int logout()
{
    pid_t pid;
    int p[2];
    
    if(pipe(p) == -1)
    {
        perror("pipe()\n");
        exit(0);
    }

    switch(pid = fork())
    {
        case -1: 
            perror("fork()\n");
            exit(0);
        case 0:
            close(p[0]);
            strcpy(mesaj, "Ati fost delogat cu succes.");
            lungime = strlen(mesaj) + 1;
            write(p[1], &lungime, sizeof(int));
            write(p[1], mesaj, lungime);
            close(p[1]);
            exit(0);
        default:
            close(p[1]);
            read(p[0], &lungime, sizeof(int));
            read(p[0], mesaj, lungime);
            close(p[0]);
            wait(NULL); // !!!
            print_(mesaj);
            return 0;
    }
    return 0;
}

int main()
{   
    int logat = 0;
    char user[1024];
    do{
        char argument[1024];
        int indice = -1; // numarul corespunzator fiecarei comenzi

        mesajIntrare(logat, user);
        citire(argument, &indice, logat);
        executare(argument, user, indice, &logat);

        printf("Apasati [ENTER] pentru a continua...");
        fgets(mesaj, 1024, stdin);
    }while(1);
    return 0;
}