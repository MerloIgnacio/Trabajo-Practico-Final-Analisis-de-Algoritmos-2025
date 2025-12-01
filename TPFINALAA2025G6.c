#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>

#define N 4   // mismas dimensiones que tu Java

int dist[N][N] = {
    {0,10,15,20},
    {10,0,35,25},
    {15,35,0,30},
    {20,25,30,0}
};


// La cota inferior hace que para cada ciudad no visitada se toma la arista saliente mas barata y se suman todos esos minimos.
// Esta cota es una estimacion optimista del costo que falta para completar el camino por todos los nodos.
// Por ende sirve para poder cortar ramas que no pueden mejorar la mejor solucion encontrada hasta el momento.
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


// Branch And Bound recursivo
void branchAndBound(int camino[], int visitados[], int nivel, int costoActual,
                    int *mejorCostoLocal, int mejorCaminoLocal[]) {

    // Caso base, si el nivel es igual a N, se ha visitado todas las ciudades
    // Osea que el camino actual contiene N ciudades y solo falta volver al origen
    if (nivel == N) {
        int ultimo = camino[nivel - 1];
        // Se calcula el costo del camino actual mas el regreso al origen para el costo total
        int costoTotal = costoActual + dist[ultimo][0]; // retorno al origen
        // Si el costo total es mejor que el costo local mejor, entonces se actualiza
        // el mejor camino encontrado por este proceso
        if (costoTotal < *mejorCostoLocal) {
            *mejorCostoLocal = costoTotal;
            for (int i = 0; i < N; i++)
            //copio el camino actual al mejor camino local
                mejorCaminoLocal[i] = camino[i];
            // Agrego el regreso al origen al final del camino
            mejorCaminoLocal[N] = 0;
        }
        return;
    }

    // primero se calcula una cota inferior optimista para esta rama
    // Por ende la cota va a ser el costo actual mas el minimo costo posible para completar el resto del camino
    int cota = costoActual + cotaInferior(visitados);
    // Si la cota es mayor o igual al mejor costo local encontrado hasta ahora, esta rama nunca va a poder mejorar la solucion
    // Por ende se poda esta rama, y no se la sigue explorando
    if (cota >= *mejorCostoLocal)
        return;

    int ultimaCiudad = camino[nivel - 1];

    for (int c = 0; c < N; c++) {
        // Para cada ciudad no visitada, se marca como visitada y se agrega al camino
        if (!visitados[c]) {

            visitados[c] = 1;
            camino[nivel] = c;

            // Llamada recursiva para el siguiente nivel, teniendo en cuenta el costo de ir a la siguiente ciudad
            branchAndBound(camino, visitados, nivel + 1,
                           costoActual + dist[ultimaCiudad][c],
                           mejorCostoLocal, mejorCaminoLocal);

            visitados[c] = 0;
        }
    }
}


// Paralelismo con fork: cada proceso hijo explora una raiz distinta (0 -> i)
int main() {
    //empiezo el mejor costo global en infinito
    int mejorCostoGlobal = INT_MAX;
    //creo el arreglo para el mejor camino global
    int mejorCaminoGlobal[N+1];

    // Procesos hijos: cada uno explora una raíz distinta (0 → siguienteCiudad)
    for (int siguienteCiudad = 1; siguienteCiudad < N; siguienteCiudad++) {
        // Pipe para que se pueda comunicar el padre con el hijo
        // el pipe es unidireccional, en pipefd[0] se lee y en pipefd[1] se escribe
        int pipefd[2];
        pipe(pipefd);

        pid_t pid = fork();

        if (pid == 0) {
            // Proceso Hijo
            // cierro el extremo de lectura del pipe, por que el hijo solo quiere escribir
            close(pipefd[0]);
            // inicializo el hijo para que explore la rama que empieza en 0 -> siguienteCiudad
            int visitados[N] = {0};
            int camino[N];
            int mejorCostoLocal = INT_MAX;
            int mejorCaminoLocal[N+1];

            visitados[0] = 1;
            visitados[siguienteCiudad] = 1;

            camino[0] = 0;
            camino[1] = siguienteCiudad;

            branchAndBound(camino, visitados, 2, dist[0][siguienteCiudad],
                           &mejorCostoLocal, mejorCaminoLocal);

            // Luego de que cada hijo explore su subarbol le envia al padre el costo + camino
            write(pipefd[1], &mejorCostoLocal, sizeof(int));
            write(pipefd[1], mejorCaminoLocal, sizeof(int) * (N+1));
            // Cierro la escritura del pipe y termino el proceso hijo
            close(pipefd[1]);
            exit(0);
        }
        else {
            // Proceso Padre, cierro el extremo de escritura del pipe, por que el padre solo quiere leer
            close(pipefd[1]);

            int costoHijo;
            int caminoHijo[N+1];
            //Se recibe el costo y el camino desde el hijo
            read(pipefd[0], &costoHijo, sizeof(int));
            read(pipefd[0], caminoHijo, sizeof(int) * (N+1));
            // Cierro la lectura del pipe y espero a que el hijo termine
            close(pipefd[0]);
            waitpid(pid, NULL, 0);
            // Si el costo recibido del hijo es mejor que el mejor costo global, se actualiza
            if (costoHijo < mejorCostoGlobal) {
                mejorCostoGlobal = costoHijo;
                // Guardo la ruta completa del hijo
                for (int i = 0; i < N+1; i++)
                    mejorCaminoGlobal[i] = caminoHijo[i];
            }
        }
    }

    printf("Mejor costo global = %d\n", mejorCostoGlobal);
    printf("Mejor ruta global = ");
    for (int i = 0; i < N+1; i++)
        printf("%d ", mejorCaminoGlobal[i]);

    printf("\n");

    return 0;
}
