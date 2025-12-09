#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "process.h"
#include "queue.h"
#include "scheduler.h"


typedef int (*select_func)(Process*, size_t, int, int);

int num_algorithms()
{
    return sizeof(algorithmsNames) / sizeof(char *);
}

int num_modalities()
{
    return sizeof(modalitiesNames) / sizeof(char *);
}

size_t initFromCSVFile(char *filename, Process **procTable)
{
    FILE *f = fopen(filename, "r");

    size_t procTableSize = 10;

    *procTable = malloc(procTableSize * sizeof(Process));
    Process *_procTable = *procTable;

    if (f == NULL)
    {
        perror("initFromCSVFile():::Error Opening File:::");
        exit(1);
    }

    char *line = NULL;
    size_t buffer_size = 0;
    size_t nprocs = 0;
    while (getline(&line, &buffer_size, f) != -1)
    {
        if (line != NULL)
        {
            Process p = initProcessFromTokens(line, ";");

            if (nprocs == procTableSize - 1)
            {
                procTableSize = procTableSize + procTableSize;
                _procTable = realloc(_procTable, procTableSize * sizeof(Process));
            }

            _procTable[nprocs] = p;

            nprocs++;
        }
    }
    free(line);
    fclose(f);
    return nprocs;
}

size_t getTotalCPU(Process *procTable, size_t nprocs)
{
    size_t total = 0;
    for (int p = 0; p < nprocs; p++)
    {
        total += (size_t)procTable[p].burst;
    }
    return total;
}

int getCurrentBurst(Process *proc, int current_time)
{
    int burst = 0;
    for (int t = 0; t < current_time; t++)
    {
        if (proc->lifecycle[t] == Running)
        {
            burst++;
        }
    }
    return burst;
}

int run_dispatcher(Process *procTable, size_t nprocs, int algorithm, int modality, int quantum)
{
    qsort(procTable, nprocs, sizeof(Process), compareArrival);

    init_queue();
    size_t duration = getTotalCPU(procTable, nprocs) + 1;

    for (int p = 0; p < nprocs; p++)
    {
        procTable[p].lifecycle = malloc(duration * sizeof(int));
        for (int t = 0; t < duration; t++)
        {
            procTable[p].lifecycle[t] = -1;
        }
        procTable[p].waiting_time = 0;
        procTable[p].return_time = 0;
        procTable[p].response_time = -1; // Se cambia a -1, se queda en bucle
        procTable[p].completed = false;
    }

    //Selecció del algoritme
    run_generic(procTable, nprocs, algorithm, modality, quantum);

    printSimulation(nprocs, procTable, duration);

    for (int p = 0; p < nprocs; p++)
    {
        destroyProcess(procTable[p]);
    }

    cleanQueue();
    return EXIT_SUCCESS;
}

void printSimulation(size_t nprocs, Process *procTable, size_t duration)
{

    printf("%14s", "== SIMULATION ");
    for (int t = 0; t < duration; t++)
    {
        printf("%5s", "=====");
    }
    printf("\n");

    printf("|%4s", "name");
    for (int t = 0; t < duration; t++)
    {
        printf("|%2d", t);
    }
    printf("|\n");

    for (int p = 0; p < nprocs; p++)
    {
        Process current = procTable[p];
        printf("|%4s", current.name);
        for (int t = 0; t < duration; t++)
        {
            printf("|%2s", (current.lifecycle[t] == Running ? "E" : current.lifecycle[t] == Bloqued ? "B"
                                                                : current.lifecycle[t] == Finished  ? "F"
                                                                                                    : " "));
        }
        printf("|\n");
    }
}

void printMetrics(size_t simulationCPUTime, size_t nprocs, Process *procTable)
{

    printf("%-14s", "== METRICS ");
    for (int t = 0; t < simulationCPUTime + 1; t++)
    {
        printf("%5s", "=====");
    }
    printf("\n");

    printf("= Duration: %ld\n", simulationCPUTime);
    printf("= Processes: %ld\n", nprocs);

    size_t baselineCPUTime = getTotalCPU(procTable, nprocs);
    double throughput = (double)nprocs / (double)simulationCPUTime;
    double cpu_usage = (double)simulationCPUTime / (double)baselineCPUTime;

    printf("= CPU (Usage): %lf\n", cpu_usage * 100);
    printf("= Throughput: %lf\n", throughput * 100);

    double averageWaitingTime = 0;
    double averageResponseTime = 0;
    double averageReturnTime = 0;
    double averageReturnTimeN = 0;

    for (int p = 0; p < nprocs; p++)
    {
        averageWaitingTime += procTable[p].waiting_time;
        averageResponseTime += procTable[p].response_time;
        averageReturnTime += procTable[p].return_time;
        averageReturnTimeN += procTable[p].return_time / (double)procTable[p].burst;
    }

    printf("= averageWaitingTime: %lf\n", (averageWaitingTime / (double)nprocs));
    printf("= averageResponseTime: %lf\n", (averageResponseTime / (double)nprocs));
    printf("= averageReturnTimeN: %lf\n", (averageReturnTimeN / (double)nprocs));
    printf("= averageReturnTime: %lf\n", (averageReturnTime / (double)nprocs));
}

size_t select_fcfs(Process *p, size_t n, int t, int q) {
    for (size_t i = 0; i < n; i++) 
        if (!p[i].completed && p[i].arrive_time <= t) return i; // FCFS: primer proceso que haya llegado y no esté completado
    return (size_t)-1; // ningún proceso disponible
}

size_t select_sjf(Process *p, size_t n, int t, int pre) {
    size_t pos = (size_t)-1; int rem = 0;
    for (size_t i = 0; i < n; i++) if (!p[i].completed && p[i].arrive_time <= t) {
        int r = pre ? p[i].burst - getCurrentBurst(&p[i], t) : p[i].burst; // preemptivo = tiempo restante, no preemptivo = burst total
        if (pos == (size_t)-1 || r < rem) { pos = i; rem = r; } // elegir el más corto
    }
    return pos; // devuelve índice del proceso más corto
}

size_t select_priority(Process *p, size_t n, int t, int pre) {
    size_t pos = (size_t)-1;
    for (size_t i = 0; i < n; i++) if (!p[i].completed && p[i].arrive_time <= t)
        if (pos == (size_t)-1 || p[i].priority < p[pos].priority) pos = i; // elegir el de mayor prioridad (menor valor numérico)
    return pos;
}

void enqueue_arrivals(Process *p, size_t n, int t, bool *enq) {
    for (size_t i = 0; i < n; i++) {
        if (!p[i].completed && p[i].arrive_time <= t && !enq[i]) {
            enqueue(&p[i]);
            enq[i] = true;
        }
    }
}

int run_generic(Process *p, size_t n, int alg, int mod, int q) {
    printf("Ejecutando %s...\n", algorithmsNames[alg]);

    int t = 0;
    size_t done = 0;

    // Marca local para saber si cada proceso ya fue encolado al menos una vez
    bool *enq = calloc(n, sizeof(bool));
    if (!enq) return -1;

    while (done < n) {
        // Encolar todo lo que ya haya llegado y no esté en cola
        enqueue_arrivals(p, n, t, enq);

        // Si no hay nada que ejecutar, avanzar el tiempo
        if (!get_queue_size()) { t++; continue; }

        // Tomar candidato de la cola
        Process *cur = dequeue();

        // Si el proceso ya se completó (pudo completarse justo al encolarse/reevaluarse), saltar
        if (cur->completed) continue;

        // Selección “mejor” proceso desde la cola para SJF/Prioridades
        if (alg == SJF || alg == PRIORITIES) {
            size_t size = get_queue_size();
            Process *best = cur;

            for (size_t i = 0; i < size; i++) {
                Process *tmp = dequeue();
                if (tmp->completed) { continue; } // ignorar completados

                int better =
                    (alg == SJF)
                        ? (tmp->burst - getCurrentBurst(tmp, t) <
                           best->burst - getCurrentBurst(best, t))
                        : (tmp->priority < best->priority);

                if (better) {
                    enqueue(best);
                    best = tmp;
                } else {
                    enqueue(tmp);
                }
            }
            cur = best;
        }

        // Ejecutar según algoritmo/modo
        if (alg == RR) {
            // Round Robin
            int rem = cur->burst - getCurrentBurst(cur, t);
            int run = (rem < q ? rem : q);

            if (cur->response_time < 0) cur->response_time = t - cur->arrive_time;

            for (int k = t; k < t + run; k++) cur->lifecycle[k] = Running;
            t += run;

            // Encolar nuevas llegadas durante el quantum (sin duplicar)
            enqueue_arrivals(p, n, t, enq);

            // ¿Terminó?
            if (cur->burst - getCurrentBurst(cur, t) == 0) {
                cur->completed = true;
                cur->return_time = t;
                cur->waiting_time = t - cur->arrive_time - cur->burst;
                done++;
            } else {
                enqueue(cur);
            }
        } else if ((alg == SJF && mod == PREEMPTIVE) ||
                   (alg == PRIORITIES && mod == PREEMPTIVE)) {
            // Un tick y reevaluar
            cur->lifecycle[t] = Running;
            if (cur->response_time < 0) cur->response_time = t - cur->arrive_time;
            t++;

            // Encolar nuevas llegadas (sin duplicar)
            enqueue_arrivals(p, n, t, enq);

            // ¿Terminó?
            if (getCurrentBurst(cur, t) == cur->burst) {
                cur->completed = true;
                cur->return_time = t;
                cur->waiting_time = t - cur->arrive_time - cur->burst;
                done++;
            } else {
                enqueue(cur);
            }
        } else {
            // FCFS / SJF no-preemptivo / Prioridades no-preemptivo
            if (cur->response_time < 0) cur->response_time = t - cur->arrive_time;

            for (int k = t; k < t + cur->burst; k++) cur->lifecycle[k] = Running;
            t += cur->burst;

            cur->completed = true;
            cur->return_time = t;
            cur->waiting_time = cur->return_time - cur->arrive_time - cur->burst;
            done++;

            // Encolar nuevas llegadas al terminar (sin duplicar)
            enqueue_arrivals(p, n, t, enq);
        }
    }

    free(enq);
    return 0;
}