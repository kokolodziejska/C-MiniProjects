#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define debug 1

#define plik_A "A.txt"
#define plik_B "B.txt"

// #define plik_A "C.txt"
// #define plik_B "D.txt"

// #define plik_A "pa.txt"
// #define plik_B "pb.txt"

pthread_mutex_t suma_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    double** A;
    double** B;
    double** C;
    int rowA, colA, rowB, colB;
    int* tab_indeksy;  
    int id_wat;  
    double *suma;      
} PrzeslijDane;


typedef struct {
    double** A;
    double** B;
    double** C;
    int* tab_indeksy;  
    int id_wat;  
    double *suma;      
} PrzeslijDanePoj;

void mnoz(double**A, int a, int b, double**B, int c, int d, double**C)
{
    int i, j, k;
    double s;
    for(i=0; i<a; i++)
    {
        for(j=0; j<d; j++)
        {
            s = 0;
            for(k=0; k<b; k++)
            {
                s+=A[i][k]*B[k][j];
            }
            C[i][j] = s;
        }

    }
}

void *mnoz_watki(void* arg)
{
    PrzeslijDane *dane = (PrzeslijDane*)arg;

    int pier = dane->tab_indeksy[dane->id_wat];
    int dru = dane->tab_indeksy[dane->id_wat + 1];
    double lokalna_suma = 0;

    for(int e=pier; e<dru; e++){
        int wie=e/dane->colB;
        int kol=e%dane->colB;
        double s=0;
        for (int i=0; i<dane->colA; i++){
            s+=dane->A[wie][i]*dane->B[i][kol];
        }
        dane->C[wie][kol]=s;
        lokalna_suma += s;
    }
    pthread_mutex_lock(&suma_mutex);
    *(dane->suma) += lokalna_suma;
    pthread_mutex_unlock(&suma_mutex);

    free(dane); 
  return NULL;
}

void *mnoz_watki_poj(void* arg)
{
    PrzeslijDanePoj *dane = (PrzeslijDanePoj*)arg;

    int pier = dane->tab_indeksy[dane->id_wat];
    int dru = dane->tab_indeksy[dane->id_wat + 1];
    double s=0;

    for (int i=pier; i<dru; i++){
        s+=(dane->A[0][i])*(dane->B[i][0]);
    }

    pthread_mutex_lock(&suma_mutex);
    *(dane->suma) += s;
    dane->C[0][0] += s;
    pthread_mutex_unlock(&suma_mutex);
    free(dane); 

  return NULL;
}


void print_matrix(double**A, int m, int n)
{
    int i, j;
    printf("[");
    for(i =0; i< m; i++)
    {
        for(j=0; j<n; j++)
        {
            printf("%f ", A[i][j]);
        }
        printf("\n");
    }
    printf("]\n");
}

int main(int argc, char **argv)
{
    //Sprwadzenie argumentów wywołania
    if (argc < 2) {
        printf( "Brakuje liczby wątków");
        return EXIT_FAILURE;
    }

    //Deklaracje
    FILE *fpa;
    FILE *fpb;
    double **A;
    double **B;
    double **C;
    int *tab_indeksy;
    int rowA, columnA, rowB, columnB;
    int i, j;
    double x;
    double sumaElementow=0.0;

    //Wczytanie pliku
    fpa = fopen(plik_A, "r");
    fpb = fopen(plik_B, "r");
    if( fpa == NULL || fpb == NULL )
    {
        perror("błąd otwarcia pliku");
        exit(-10);
    }

    //Wczytanie wymiarów tablic
    fscanf (fpa, "%d", &rowA);
    fscanf (fpa, "%d", &columnA);

    fscanf (fpb, "%d", &rowB);
    fscanf (fpb, "%d", &columnB);

    printf("Macierz A ma rozmiar %d x %d, a B %d x %d\n",rowA, columnA, rowB, columnB);

    //Sprawdzenie czy można pomnożyć
    if( columnA != rowB)
    {
        printf("Złe wymiary macierzy!\n");
        return EXIT_FAILURE;
    }
    
    //Alokacja pamieci dla tablicy A i B
    A = malloc(rowA*sizeof(double));
    for(i=0; i< rowA; i++)
    {
        A[i] = malloc(columnA*sizeof(double));
    }

    B = malloc(rowB*sizeof(double));
    for(i=0; i< rowB; i++)
    {
        B[i] = malloc(columnB*sizeof(double));
    }

    //Ptrzygotownie macierzy wynikowej
    C = malloc(rowA *sizeof(double));
    for(i=0; i< rowA; i++)
    {
        C[i] = malloc(columnB*sizeof(double));
    }

    // Inicjalizacja macierzy wynikowej
    for(i=0; i<rowA; i++)
    {
        for(j=0; j<columnB; j++)
        {
            C[i][j] = 0;
        }
    }

    //wczytanie macierzy z pliku
    printf("RRozmiar wynikowej macierzy C: %dx%d\n\n", rowA, columnB );

    for(i =0; i< rowA; i++)
    {
        for(j = 0; j<columnA; j++)
        {
            fscanf( fpa, "%lf", &x );
            A[i][j] = x;
        }
    }

    printf("A:\n");
    print_matrix(A, rowA, columnA);

    for(i =0; i< rowB; i++)
    {
        for(j = 0; j<columnB; j++)
        {
            fscanf( fpb, "%lf", &x );
            B[i][j] = x;
        }
    }

    printf("B:\n");
    print_matrix(B, rowB, columnB);


    

    //Specjalny przypadek, gdy mamy tablicę wynikową o wymiarze 1x1
    if ((rowA*columnB) ==1){

        //Podział danych obliczniowych
        int ilosc_watkow_pobrany = atoi(argv[1]);
        int ilosc_watkow;

        if (ilosc_watkow_pobrany>columnA){
            ilosc_watkow = columnA;
            printf("Za dużo wątków, program zostanie podzielony na: %d\n", ilosc_watkow);
        }
        else{
            ilosc_watkow = ilosc_watkow_pobrany;
        }

        tab_indeksy = malloc((ilosc_watkow +1) *sizeof(int));

        int suma = 0;
        int n=columnA;
        int base_size = n / ilosc_watkow;
        int reszta    = n % ilosc_watkow;
    
        tab_indeksy[0] = 0;
    
        for (int i = 1; i < ilosc_watkow + 1; i++) {
            if (reszta > 0) {
                suma += (base_size + 1);
                reszta--;
            } else {
                suma += base_size;
            }
            tab_indeksy[i] = suma;
        }

        //sprawdzenie podziału
        if(debug){
            printf("tab_indeksy: ");
            for (int i = 0; i < ilosc_watkow + 1; i++) {
                printf("%d ", tab_indeksy[i]);
            }
            printf("\n");
        }
        pthread_t watki[ilosc_watkow];
        int watki_ids[ilosc_watkow];

        for (int i = 0; i < ilosc_watkow; i++) {
            watki_ids[i] = i; 
            PrzeslijDanePoj *dane = malloc(sizeof(PrzeslijDanePoj));
            dane->A = A;
            dane->B = B;
            dane->C = C;
            dane->tab_indeksy = tab_indeksy;
            dane->id_wat = i;
            dane->suma = &sumaElementow;

            if (pthread_create(&watki[i], NULL, mnoz_watki_poj, (void*)dane) != 0) {
                perror("Błąd tworzenia wątku");
                exit(EXIT_FAILURE);
            }
        }

        // Zebranie statusów zakończenia wątków
        for (i = 0; i < ilosc_watkow; i++) {
            if (pthread_join(watki[i], NULL) != 0) {
                perror("Błąd dołączania wątku");
                exit(EXIT_FAILURE);
            }
        }
    }
    else{
        //Podział danych obliczniowych
        int ilosc_watkow_pobrany = atoi(argv[1]);
        int ilosc_watkow;

        if (ilosc_watkow_pobrany>(rowA*columnB)){
            ilosc_watkow = (rowA*columnB);
            printf("Za dużo procesów, program zostanie podzielony na: %d\n", ilosc_watkow);
        }
        else{
            ilosc_watkow = ilosc_watkow_pobrany;
        }

        tab_indeksy = malloc((ilosc_watkow +1) *sizeof(int));

        int suma = 0;
        int n=rowA*columnB;
        int base_size = n / ilosc_watkow;
        int reszta    = n % ilosc_watkow;

        tab_indeksy[0] = 0;

        for (int i = 1; i < ilosc_watkow + 1; i++) {
            if (reszta > 0) {
                suma += (base_size + 1);
                reszta--;
            } else {
                suma += base_size;
            }
            tab_indeksy[i] = suma;
        }

        //sprawdzenie podziału
        if(debug){
            printf("tab_indeksy: ");
            for (int i = 0; i < ilosc_watkow + 1; i++) {
                printf("%d ", tab_indeksy[i]);
            }
            printf("\n");
        }
        

        //Utworzenie wątków potomnych
        pthread_t watki[ilosc_watkow];
        int watki_ids[ilosc_watkow];

        for (int i = 0; i < ilosc_watkow; i++) {
            watki_ids[i] = i; 
            PrzeslijDane* dane = malloc(sizeof(PrzeslijDane));
            dane->A = A;
            dane->B = B;
            dane->C = C;
            dane->rowA = rowA;
            dane->colA = columnA;
            dane->rowB = rowB;
            dane->colB = columnB;
            dane->tab_indeksy = tab_indeksy;
            dane->id_wat = i;
            dane->suma = &sumaElementow;

            if (pthread_create(&watki[i], NULL, mnoz_watki, (void*)dane) != 0) {
                perror("Błąd tworzenia wątku");
                exit(EXIT_FAILURE);
            }
        }

        // Zebranie statusów zakończenia wątków
        for (i = 0; i < ilosc_watkow; i++) {
            if (pthread_join(watki[i], NULL) != 0) {
                perror("Błąd dołączania wątku");
                exit(EXIT_FAILURE);
            }
        }
    }

    printf("C:\n");
    print_matrix(C, rowA,columnB);
    printf("Suma elementów macierzy C: %f\n", sumaElementow);

    //Mnożenie

    if(debug){
          
        double **D;
        D = malloc(rowA *sizeof(double));
        for(i=0; i< rowA; i++)
        {
            D[i] = malloc(columnB*sizeof(double));
        }
        mnoz(A, rowA, columnA, B, rowB, columnB, D);

        printf("Macierz liczona tradycjnie D:\n");
        print_matrix(D, rowA,columnB);

        for(i=0; i<rowA; i++)
        {
            free(D[i]);
        }
        free(D);

    }

    //Zwalnianie pamięci
    for(i=0; i<rowA; i++)
    {
        free(A[i]);
    }
    free(A);

    for(i=0; i<rowB; i++)
    {
        free(B[i]);
    }
    free(B);

    for(i=0; i<rowA; i++)
    {
        free(C[i]);
    }
    free(C);

    free(tab_indeksy);
    fclose(fpa);
    fclose(fpb);

    return 0;
}