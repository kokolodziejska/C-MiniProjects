#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#define plik_wektor "wektor.txt"
#define debug 1

double sum(double* vector, int p, int n) {
    double s = 0.0f;
    for(int i = p; i < n; i++) {
        s += vector[i];
    }
    return s;
}

void on_usr1(int signo){
    return;
}

int main(int argc, char **argv){ 
    if (argc < 2) {
        fprintf(stderr, "Uzycie: %s <ilosc_procesow>\n", argv[0]);
        return 1;
    }

    int ilosc_procesow = atoi(argv[1]);

    // Zmienne do pamięci współdzielonej
    int shmid_indexy;
    int shmid_sumy;
    int shmid_wektor;
    key_t key_indeksy = ftok(plik_wektor, 65);
    key_t key_sumy    = ftok(plik_wektor, 66);
    key_t key_wektor  = ftok(plik_wektor, 67);

    // Rozmiary
    size_t size_indeksy = sizeof(int) * (ilosc_procesow + 1);
    size_t size_sumy    = sizeof(double) * ilosc_procesow;
    size_t size_wektor; 

    // Wskaźniki do segmentów
    int  *tab_indeksy;
    double  *tab_sumy;
    double *vector_shm;

    // 1. Tworzenie procesów potomnych
    pid_t pids[ilosc_procesow];
    for(int i = 0; i < ilosc_procesow; i++){
        pid_t pid = fork();
        switch(pid){
            case -1:
                perror("fork failed");
                return 1;

            case 0: {
                
                // 2. Konfiguracja sygnału SIGUSR1
                sigset_t mask;
                struct sigaction usr1;
                sigemptyset(&mask);
                usr1.sa_handler = &on_usr1;
                usr1.sa_mask = mask;
                usr1.sa_flags = SA_SIGINFO;
                sigaction(SIGUSR1, &usr1, NULL);

                // 3. Oczekiwanie na sygnał SIGUSR1
                pause();

                if((shmid_indexy = shmget(key_indeksy, size_indeksy, 0666 | IPC_CREAT)) == -1){
                    perror("blad w shmget indeksy potomka");
                    exit(1);
                }
                tab_indeksy = (int*) shmat(shmid_indexy, NULL, 0);
                if(tab_indeksy == (void*) -1){
                    perror("blad w shmat indeksy potomka");
                    exit(1);
                }

                int poczatek = tab_indeksy[i];
                int koniec   = tab_indeksy[i + 1];
                if (shmdt(tab_indeksy) == -1) {
                    perror("blad w shmdt indeksy potomka");
                    exit(1);
                }
                if(debug){
                    printf("Proces potomny %d, index potomka = %d, indeks poczatkowy %d, indeks koncowy %d\n",
                        getpid(), i, poczatek, koniec);
                }

                if ((shmid_wektor = shmget(key_wektor, 0, 0666)) == -1){
                    perror("blad w shmget wektor potomka");
                    exit(1);
                }
                double *vector_local = (double*) shmat(shmid_wektor, NULL, 0);
                if (vector_local == (void*)-1) {
                    perror("blad w shmat wektor potomka");
                    exit(1);
                }

                double wynik = sum(vector_local, poczatek, koniec);
                if(debug){
                    printf("Suma w procesie %d: %f\n", getpid(), wynik);
                }

                if (shmdt(vector_local) == -1) {
                    perror("blad w shmdt wektor potomka");
                    exit(1);
                }

                if((shmid_sumy = shmget(key_sumy, size_sumy, 0666 | IPC_CREAT)) == -1){
                    perror("blad w shmget sumy potomka");
                    exit(1);
                }
                tab_sumy = (double*) shmat(shmid_sumy, NULL, 0);
                if(tab_sumy == (void*) -1){
                    perror("blad w shmat sumy potomka");
                    exit(1);
                }

                tab_sumy[i] = wynik;


                if (shmdt(tab_sumy) == -1) {
                    perror("blad w shmdt sumy potomka");
                    exit(1);
                }

                exit(0);
            }

            default:
                pids[i] = pid;
        }
    }

    // 4. Proces główny czyta plik z wektorem (wielkość n)
    FILE* f = fopen(plik_wektor, "r");
    if (!f) {
        perror("blad w otwarciu pliku");
        return 1;
    }
    int n;
    if (fscanf(f, "%d", &n) != 1) {
        fprintf(stderr, "blad odczytu wielkosci wektora\n");
        fclose(f);
        return 1;
    }

    // 4.a) Tworzenie pamieci wspoldzielonej dla indeksow
    if ((shmid_indexy = shmget(key_indeksy, size_indeksy, 0666 | IPC_CREAT)) == -1) {
        perror("blad w shmget indeksy");
        return 1;
    }

    // 4.b) Tworzenie pamieci wspoldzielonej dla sum
    if ((shmid_sumy = shmget(key_sumy, size_sumy, 0666 | IPC_CREAT)) == -1) {
        perror("blad shmget sumy");
        return 1;
    }

    // 4.c) Tworzenie pamieci wspoldzielonej dla wektora
    size_wektor = sizeof(double) * n;
    shmid_wektor = shmget(key_wektor, size_wektor, 0666 | IPC_CREAT);
    if (shmid_wektor == -1) {
        perror("blad shmget wektor");
        return 1;
    }

    // 5. Uzupełnianie wektora w pamieci współdzielonej
    vector_shm = (double*) shmat(shmid_wektor, NULL, 0);
    if (vector_shm == (void*)-1) {
        perror("blad w shmat wektor (rodzic)");
        fclose(f);
        return 1;
    }

    for (int i = 0; i < n; i++) {
        if (fscanf(f, "%lf", &vector_shm[i]) != 1) {
            fprintf(stderr, "blad czytania elementu %d wektora\n", i);
            fclose(f);
            shmdt(vector_shm);
            return 1;
        }
    }
    fclose(f);

    // 6. Obliczanie indeksów
    tab_indeksy = (int*) shmat(shmid_indexy, NULL, 0);
    if (tab_indeksy == (void *) -1) {
        perror("blad w shmat indeksy");
        return 1;
    }

    int suma = 0;
    int base_size = n / ilosc_procesow;
    int reszta    = n % ilosc_procesow;
    tab_indeksy[0] = 0;

    for (int i = 1; i < ilosc_procesow + 1; i++) {
        if (reszta > 0) {
            suma += (base_size + 1);
            reszta--;
        } else {
            suma += base_size;
        }
        tab_indeksy[i] = suma;
    }
    if(debug){
        printf("tab_indeksy: ");
        for (int i = 0; i < ilosc_procesow + 1; i++) {
            printf("%d ", tab_indeksy[i]);
        }
        printf("\n");
    }

    // 7. Inicjalizacja sum
    tab_sumy = (double*) shmat(shmid_sumy, NULL, 0);
    if (tab_sumy == (void *) -1) {
        perror("blad shmat sumy");
        return 1;
    }
    for(int i = 0; i < ilosc_procesow; i++){
        tab_sumy[i] = 0;
    }
    if(debug){
        printf("tab_sumy: ");
        for (int i = 0; i < ilosc_procesow; i++) {
            printf("%f ", tab_sumy[i]);
        }
        printf("\n");
    }
    usleep(5000);

    // 8. Wysyłanie sygnału SIGUSR1 do procesów potomnych
    for(int i = 0; i < ilosc_procesow; i++){
        kill(pids[i], SIGUSR1);
    }

    // 9. Oczekiwanie na procesy potomne
    for(int i = 0; i < ilosc_procesow; i++){
        if(wait(NULL) == -1) {
            perror("blad w wait");
            return 1;
        }
    }

    // 10. Sumowanie sum
    double suma_koncowa = 0.0;
    for(int i = 0; i < ilosc_procesow; i++){
        suma_koncowa += tab_sumy[i];
    }

    printf("Końcowa suma elementów w wektorze = %.2f\n", suma_koncowa);

    // 11. Odłączanie pamięci współdzielonej
    if (shmdt(tab_indeksy) == -1) {
        perror("blad w shmdt indeksy (rodzic)");
    }
    if (shmdt(tab_sumy) == -1) {
        perror("blad w shmdt sumy (rodzic)");
    }
    if (shmdt(vector_shm) == -1) {
        perror("blad w shmdt wektor (rodzic)");
    }

    // 12. Usuwanie pamięci współdzielonej
    if (shmctl(shmid_indexy, IPC_RMID, NULL) == -1) {
        perror("blad w shmctl indeksy");
    }
    if (shmctl(shmid_sumy, IPC_RMID, NULL) == -1) {
        perror("blad w shmctl sumy");
    }
    if (shmctl(shmid_wektor, IPC_RMID, NULL) == -1) {
        perror("blad w shmctl wektor");
    }

    return 0;
}
