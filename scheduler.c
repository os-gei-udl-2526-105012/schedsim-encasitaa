#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "process.h"
#include "queue.h"
#include "scheduler.h"

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

    Process *_proclist;

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
        procTable[p].response_time = 0;
        procTable[p].completed = false;
    }

    //Selecció del algoritme
    switch (algorithm)
    {
    case FCFS:
        fcfs(procTable, nprocs);
        break;
    case SJF:
        sjf(procTable, nprocs, modality == PREEMPTIVE ? 1 : 0);
        break;
    case RR:
        rr(procTable, nprocs, quantum);
        break;
    case PRIORITIES:
        priority(procTable, nprocs, modality);
        break;
    default:
        fprintf(stderr, "Algoritmo desconocido: %d\n", algorithm);
        break;
    }

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

static int fcfs(Process *procTable, size_t nprocs)
{
    printf("Ejecutando FCFS...\n");

    int current_time = 0;

    for (size_t p = 0; p < nprocs; p++)
    {
        // Esperar si el proceso llega más tarde que el tiempo actual
        if (procTable[p].arrive_time > current_time)
        {
            current_time = procTable[p].arrive_time;
        }

        // Ejecutar el proceso desde current_time hasta que termine
        for (int t = current_time; t < current_time + procTable[p].burst; t++)
        {
            procTable[p].lifecycle[t] = Running;
        }

        procTable[p].completed = true;
        procTable[p].response_time = current_time - procTable[p].arrive_time;
        procTable[p].waiting_time = current_time - procTable[p].arrive_time;
        current_time += procTable[p].burst;
        procTable[p].return_time = current_time;
    }

    return 0;
}

static int sjf(Process *procTable, size_t nprocs, int preemptive)
{
    printf("Ejecutando %s...\n", preemptive ? "SJRT (preemptivo)" : "SJF (no preemptivo)");

    int current_time = 0;
    size_t completed = 0;

    while (completed < nprocs)
    {
        int best = -1;
        for (size_t p = 0; p < nprocs; p++)
        {
            if (!procTable[p].completed && procTable[p].arrive_time <= current_time)
            {
                if (best == -1 || procTable[p].burst < procTable[best].burst)
                {
                    best = (int)p;
                }
            }
        }

        if (best == -1)
        {
            // No hay proceso disponible todavía, avanzamos el tiempo
            current_time++;
            continue;
        }

        Process *proc = &procTable[best];

        // Ejecutar el proceso seleccionado
        for (int t = current_time; t < current_time + proc->burst; t++)
        {
            proc->lifecycle[t] = Running;
        }

        proc->completed = true;
        proc->response_time = current_time - proc->arrive_time;
        proc->waiting_time = proc->response_time;
        current_time += proc->burst;
        proc->return_time = current_time;

        completed++;
    }

    return 0;
}

static int rr(Process *procTable, size_t nprocs, int quantum)
{
    printf("Ejecutando Round Robin con quantum=%d...\n", quantum);
    int current_time = 0;
    size_t completed = 0;

    init_queue();

    while (completed < nprocs){
        //afegim processos que arriben en aquest instant
        for (size_t p = 0; p < nprocs; p++){
            if (!procTable[p].completed && procTable[p].arrive_time == current_time){
                enqueue(&procTable[p]);
            }
        }

        if (isEmpty()){
            current_time++;
            continue;
        }

        Process *proc = dequeue();

        //si és la primera vegada que s’executa, calculem response_time
        if (proc->response_time == 0 && current_time >= proc->arrive_time){
            proc->response_time = current_time - proc->arrive_time;
        }

        int run_time = (proc->burst - getCurrentBurst(proc, current_time) < quantum) ? proc->burst - getCurrentBurst(proc, current_time): quantum;

        for (int t = current_time; t < current_time + run_time; t++){
            proc->lifecycle[t] = Running;
        }

        current_time += run_time;

        //afegim processos que arriben mentre aquest s’executava
        for (size_t p = 0; p < nprocs; p++){
            if (!procTable[p].completed && procTable[p].arrive_time > current_time - run_time && procTable[p].arrive_time <= current_time){
                enqueue(&procTable[p]);
            }
        }

        if (getCurrentBurst(proc, current_time) == proc->burst){
            proc->completed = true;
            proc->return_time = current_time;
            proc->waiting_time = proc->return_time - proc->arrive_time - proc->burst;
            completed++;
        }
        else{
            enqueue(proc);
        }
    }
    return 0;
}

static int priority(Process *procTable, size_t nprocs, int preemptive)
{
    printf("Ejecutando Prioridad %s...\n", preemptive ? "(preemptivo)" : "(no preemptivo)");
    int current_time = 0;
    size_t completed = 0;

    while (completed < nprocs){
        int best = -1;
        for (size_t p = 0; p < nprocs; p++){
            if (!procTable[p].completed && procTable[p].arrive_time <= current_time){
                if (best == -1 || procTable[p].priority < procTable[best].priority){
                    best = (int)p;
                }
            }
        }

        if (best == -1){
            current_time++;
            continue;
        }

        Process *proc = &procTable[best];

        if (!preemptive){
            //no preemptiu: executa fins acabar
            for (int t = current_time; t < current_time + proc->burst; t++){
                proc->lifecycle[t] = Running;
            }
            proc->completed = true;
            proc->response_time = current_time - proc->arrive_time;
            proc->waiting_time = proc->response_time;
            current_time += proc->burst;
            proc->return_time = current_time;
            completed++;
        }
        else{
            //preemptiu: executa 1 unitat i torna a decidir
            proc->lifecycle[current_time] = Running;
            if (getCurrentBurst(proc, current_time + 1) == proc->burst){
                proc->completed = true;
                proc->return_time = current_time + 1;
                proc->waiting_time = proc->return_time - proc->arrive_time - proc->burst;
                completed++;
            }
            current_time++;
        }
    }
    return 0;
}