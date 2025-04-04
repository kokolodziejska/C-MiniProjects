#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define MAX_THREADS 8

double run_sequential(int n, int** primes_out, int* count_out) {
    int S = (int)sqrt(n);
    long int* a = malloc((S + 1) * sizeof(long int));
    int* primes = malloc(n * sizeof(int));
    long i, k, liczba, reszta;
    long int lpodz, llpier = 0;

    double start = omp_get_wtime();

    for (i = 2; i <= S; i++) a[i] = 1;
    for (i = 2; i <= S; i++) {
        if (a[i] == 1) {
            primes[llpier++] = i;
            for (k = i + i; k <= S; k += i)
                a[k] = 0;
        }
    }

    lpodz = llpier;
    for (liczba = S + 1; liczba <= n; liczba++) {
        for (k = 0; k < lpodz; k++) {
            reszta = (liczba % primes[k]);
            if (reszta == 0) break;
        }
        if (reszta != 0)
            primes[llpier++] = liczba;
    }

    double end = omp_get_wtime();

    *primes_out = primes;
    *count_out = llpier;

    free(a);
    return end - start;
}

double run_parallel(int n, int num_threads, int** primes_out, int* count_out) {
    omp_set_num_threads(num_threads);
    int S = (int)floor(sqrt(n));
    char* pocz_pier = calloc(S + 1, sizeof(char));
    int* wszytk_pier = malloc(n * sizeof(int));
    int* podzielniki = malloc((S + 1) * sizeof(int));
    int liczba_podzielniki = 0;
    int liczba_pier = 0;

    double start_time = omp_get_wtime();

    
    for (int i = 2; i <= S; i++) pocz_pier[i] = 1;
    for (int i = 2; i * i <= S; ++i) {
        if (pocz_pier[i]) {
            for (int j = i + i; j <= S; j += i) {
                pocz_pier[j] = 0;
            }
        }
    }

    for (int i = 2; i <= S; ++i) {
        if (pocz_pier[i]) {
            podzielniki[liczba_podzielniki++] = i;
            wszytk_pier[liczba_pier++] = i;
        }
    }

 
    #pragma omp parallel
    {
        int* local_pier = malloc((n - S + 1) * sizeof(int));
        int local_liczba = 0;
        int reszta;

        #pragma omp for schedule(dynamic)
        for (int liczba = S + 1; liczba <= n; liczba++) {
            for (int k = 0; k < liczba_podzielniki; k++) {
                reszta = liczba % podzielniki[k];
                if (reszta == 0) break;
            }
            if (reszta != 0)
                local_pier[local_liczba++] = liczba;
        }

        int offset;
        #pragma omp critical
        {
            offset = liczba_pier;
            liczba_pier += local_liczba;
        }

        for (int i = 0; i < local_liczba; ++i) {
            wszytk_pier[offset + i] = local_pier[i];
        }

        free(local_pier);
    }

    double end_time = omp_get_wtime();

    *primes_out = wszytk_pier;
    *count_out = liczba_pier;

    free(pocz_pier);
    free(podzielniki);
    return end_time - start_time;
}

void save_primes_to_file(int* primes, int count, const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        perror("Nie można otworzyć pliku primes.txt");
        return;
    }

    for (int i = 0; i < count; ++i) {
        fprintf(f, "%d\n", primes[i]);
    }

    fclose(f);
}

int main() {
    int sizes[] = {100000, 1000000, 10000000};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int i = 0; i < num_sizes; ++i) {
        int n = sizes[i];
        int* primes_seq = NULL;
        int count_seq = 0;

        printf("===== Test dla n = %d =====\n", n);
        double time_seq = run_sequential(n, &primes_seq, &count_seq);
        printf("[Sekwencyjny] Czas: %.6f s | Liczb pierwszych: %d\n", time_seq, count_seq);
        free(primes_seq);

        for (int threads = 2; threads <= MAX_THREADS; ++threads) {
            int* primes_par = NULL;
            int count_par = 0;

            double time_par = run_parallel(n, threads, &primes_par, &count_par);
            double speedup = time_seq / time_par;
            double efficiency = speedup / threads;

            printf("[Równoległy - %d wątki] Czas: %.6f s | Liczb pierwszych: %d | Przyspieszenie: %.2fx | Efektywność: %.2f\n",
                   threads, time_par, count_par, speedup, efficiency);

            if (i == num_sizes - 1 && threads == MAX_THREADS) {
                save_primes_to_file(primes_par, count_par, "primes.txt");
                printf("Zapisano %d liczb pierwszych do primes.txt\n", count_par);
            }

            free(primes_par);
        }

        printf("\n");
    }

    return 0;
}