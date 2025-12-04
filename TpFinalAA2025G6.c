#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>

#define N 4  

// Matriz de distancias entre ciudades
int dist[N][N] = {
    {0,10,15,20},
    {10,0,35,25},
    {15,35,0,30},
    {20,25,30,0}
};

// Calcula una cota inferior "optimista" para decidir si seguir explorando o podar.
// Para cada ciudad no visitada toma la arista más barata que sale de ella.
int cotaInferior(int visitados[]) {
    int sum = 0;

    for (int city = 0; city < N; city++) {
        if (!visitados[city]) {
            int min = INT_MAX;
            for (int j = 0; j < N; j++) {
                if (city != j && dist[city][j] < min)
                    min = dist[city][j];
            }
            sum += min;
        }
    }
    return sum;
}

// Branch and Bound recursivo clásico para el TSP
void branchAndBound(int camino[], int visitados[], int nivel, int costoActual,
                    int *mejorCostoLocal, int mejorCaminoLocal[]) {

    // Si ya pasamos por todas las ciudades, calculamos el costo total del tour
    if (nivel == N) {
        int ultimo = camino[nivel - 1];
        int costoTotal = costoActual + dist[ultimo][0]; // volver al origen

        // Actualizar el mejor camino del proceso si este es más barato
        if (costoTotal < *mejorCostoLocal) {
            *mejorCostoLocal = costoTotal;
            for (int i = 0; i < N; i++)
                mejorCaminoLocal[i] = camino[i];
            mejorCaminoLocal[N] = 0; // regreso al origen
        }

    } else {

        // Cota inferior para podar ramas que no pueden mejorar la solución
        int cota = costoActual + cotaInferior(visitados);

        if (cota < *mejorCostoLocal) {
            int ultimaCiudad = camino[nivel - 1];

            // Intentar ir a cualquier ciudad que aún no esté visitada
            for (int c = 0; c < N; c++) {
                if (!visitados[c]) {

                    visitados[c] = 1;
                    camino[nivel] = c;

                    branchAndBound(camino, visitados, nivel + 1,
                                   costoActual + dist[ultimaCiudad][c],
                                   mejorCostoLocal, mejorCaminoLocal);

                    visitados[c] = 0; // backtracking
                }
            }
        }
    }
}


// Paralelización con fork: cada proceso explora un subárbol diferente del TSP
int main() {

    struct timeval inicio, fin;
    gettimeofday(&inicio, NULL); // arranca el contador de tiempo

    int mejorCostoGlobal = INT_MAX;
    int mejorCaminoGlobal[N+1];

    printf("Nodo origen: 0\n\n");
    printf("Matriz de distancias:\n\n");

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++)
            printf("%3d ", dist[i][j]);
        printf("\n");
    }

    printf("\nCreando procesos hijos...\n");

    // Se usa un pipe distinto por cada hijo, así cada uno manda su solución
    int pipefd[N][2];
    pid_t hijos[N];
    int numHijos = 0;

    // Cada proceso hijo explora un camino que comienza 0 -> siguienteCiudad
    for (int siguienteCiudad = 1; siguienteCiudad < N; siguienteCiudad++) {

        if (dist[0][siguienteCiudad] != 0) {

            int idx = numHijos;
            pipe(pipefd[idx]); // pipe exclusivo del hijo idx

            pid_t pid = fork();

            if (pid == 0) {
                // ----------------- PROCESO HIJO -----------------

                close(pipefd[idx][0]); // este proceso solo escribe

                printf("[HIJO %d] Explorando rama desde 0 -> %d\n",
                        getpid(), siguienteCiudad);

                int visitados[N] = {0};
                int camino[N];
                int mejorCostoLocal = INT_MAX;
                int mejorCaminoLocal[N+1];

                visitados[0] = 1;
                visitados[siguienteCiudad] = 1;

                camino[0] = 0;
                camino[1] = siguienteCiudad;

                // El hijo ejecuta su búsqueda completa
                branchAndBound(camino, visitados, 2, dist[0][siguienteCiudad],
                               &mejorCostoLocal, mejorCaminoLocal);

                printf("[HIJO %d] Terminé. Mejor costo local = %d\n",
                        getpid(), mejorCostoLocal);

                // Enviar el resultado al proceso padre
                write(pipefd[idx][1], &mejorCostoLocal, sizeof(int));
                write(pipefd[idx][1], mejorCaminoLocal, sizeof(int)*(N+1));

                close(pipefd[idx][1]);
                exit(0);

            } else {
                // ----------------- PROCESO PADRE -----------------

                close(pipefd[idx][1]); // el padre solo lee

                printf("[PADRE] Proceso hijo creado (pid = %d) para ciudad %d\n",
                        pid, siguienteCiudad);

                hijos[idx] = pid;
                numHijos++;
            }
        }
    }

    printf("\nPADRE: Esperando resultados...\n\n");

    // El padre ahora junta todos los resultados de los hijos
    for (int i = 0; i < numHijos; i++) {

        int costoHijo;
        int caminoHijo[N+1];

        // read se bloquea hasta que el hijo escriba su solución
        read(pipefd[i][0], &costoHijo, sizeof(int));
        read(pipefd[i][0], caminoHijo, sizeof(int)*(N+1));
        close(pipefd[i][0]);

        waitpid(hijos[i], NULL, 0); // esperar que termine formalmente

        printf("[PADRE] Recibí costo %d del hijo %d\n", costoHijo, hijos[i]);

        // Actualización del mejor resultado global
        if (costoHijo < mejorCostoGlobal) {
            mejorCostoGlobal = costoHijo;
            for (int j = 0; j < N+1; j++)
                mejorCaminoGlobal[j] = caminoHijo[j];
        }
    }

    printf("Mejor costo global = %d\n", mejorCostoGlobal);
    printf("Mejor ruta global = ");
    for (int i = 0; i < N+1; i++)
        printf("%d ", mejorCaminoGlobal[i]);
    printf("\n");



    return 0;
}
