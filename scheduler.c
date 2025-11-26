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

int fcfs(Process *procTable, size_t nprocs)
{
    printf("Ejecutando FCFS...\n");

    int current_time = 0;

    for (size_t p = 0; p < nprocs; p++)
    {
        enqueue(&procTable[p]); //com que els procesos ja venen ordenats per el qsort els afegim en la cua tots directament
    }

    while (get_queue_size() > 0)
    {
        Process *proc = dequeue();

        if (current_time < proc->arrive_time) current_time = proc->arrive_time;

        for (int t = current_time; t < current_time + proc->burst; t++)  //executem fins a finalitzar
        {
            proc->lifecycle[t] = Running;
        }

        proc->completed = true;
        proc->response_time = current_time - proc->arrive_time;
        proc->waiting_time = proc->response_time;
        current_time += proc->burst;
        proc->return_time = current_time;
    }

    return 0;
}

int sjf(Process *procTable, size_t nprocs, int preemptive)
{
    printf("Ejecutando %s...\n", preemptive ? "SJRT (preemptivo)" : "SJF (no preemptivo)");

    int current_time = 0;
    size_t completed = 0;

    while (completed < nprocs) {
        int posBest = -1;  //posició del millor 
        int best_remaining = 0;  //temps restant del millor procés

        for (size_t p = 0; p < nprocs; p++) {

            //Si el proces no ha arribat o ja ha finalitzat continuem a la seguent iteració
            if (procTable[p].completed || procTable[p].arrive_time > current_time) continue;

            int remaining;

            if (preemptive) {
                //SJRT - Temps restant - el ja executat 
                remaining = procTable[p].burst - getCurrentBurst(&procTable[p], current_time);
            }
            else {
                //SJF - Directament el burst total, executarem proces fins el final
                remaining = procTable[p].burst;
            }

            if (posBest == -1 || remaining < best_remaining) { //Elegim el que tingui menys temps restant
                posBest = (int)p;
                best_remaining = remaining;
            }
        }

        Process *proc = &procTable[posBest];

        if (!preemptive){
            //SJF no preemptive
            for (int t = current_time; t < current_time + proc->burst; t++) {
                proc->lifecycle[t] = Running;  //fem el matex que abans i posem running fins a finalitzar procés
            }

            proc->completed = true;
            proc->response_time = current_time - proc->arrive_time;
            proc->waiting_time = proc->response_time;
            current_time += proc->burst;
            proc->return_time = current_time;
            completed++;
        }
        else {
            //SJRT (com el sjf pero mirem el més eficient que entra en cada instant de temps)

            int executed_before = getCurrentBurst(proc, current_time); //guardem per saber si el proces ja ha sigut executat
            if (executed_before == 0 && current_time >= proc->arrive_time) {
                proc->response_time = current_time - proc->arrive_time;  //si no ha sigut executat previament, guardem quan s'ha tardat en atendre desde que ha entrat
            }

            proc->lifecycle[current_time] = Running;

            if (getCurrentBurst(proc, current_time + 1) == proc->burst) { //Si en el proxim tick ja s'ha consumit burst donem proces per completat
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

int rr(Process *procTable, size_t nprocs, int quantum)
{
    printf("Ejecutando Round Robin con quantum=%d...\n", quantum);
    int current_time = 0;
    size_t completed = 0;

    //fem servir _proclist com a array de punters a processos
    Process* _proclist[nprocs];
    size_t ready_count = 0;

    while (completed < nprocs) {
        //afegim processos que arriben en aquest instant
        for (size_t p = 0; p < nprocs; p++){
            if (!procTable[p].completed && procTable[p].arrive_time == current_time){
                _proclist[ready_count++] = &procTable[p];
            }
        }

        if (ready_count == 0){
            current_time++;
            continue;
        }

        //agafem el primer procés de la llista
        Process *proc = _proclist[0];

        //desplacem la resta cap a l’esquerra (com una cua)
        for (size_t i = 1; i < ready_count; i++){
            _proclist[i-1] = _proclist[i];
        }
        ready_count--;

        //temps de resposta si és la primera execució
        if (proc->response_time < 0 && current_time >= proc->arrive_time){
            proc->response_time = current_time - proc->arrive_time;
        }

        int remaining = proc->burst - getCurrentBurst(proc, current_time);
        int run_time  = (remaining < quantum) ? remaining : quantum;

        for (int t = current_time; t < current_time + run_time; t++){
            proc->lifecycle[t] = Running;
        }
        current_time += run_time;

        //afegim processos que han arribat mentre aquest s’executava
        for (size_t p = 0; p < nprocs; p++){
            if (!procTable[p].completed && procTable[p].arrive_time > (current_time - run_time) && procTable[p].arrive_time <= current_time){
                _proclist[ready_count++] = &procTable[p];
            }
        }

        //comprovem si ha acabat
        int remAfter = proc->burst - getCurrentBurst(proc, current_time);
        if (remAfter == 0){
            proc->completed    = true;
            proc->return_time  = current_time;
            proc->waiting_time = proc->return_time - proc->arrive_time - proc->burst;
            completed++;
        } else{
            //si no ha acabat, el tornem a posar al final de la llista
            _proclist[ready_count++] = proc;
        }
    }
    return 0;
}

int priority(Process *procTable, size_t nprocs, int preemptive)
{
    printf("Ejecutando Prioridad %s...\n", preemptive ? "(preemptivo)" : "(no preemptivo)");
    int current_time = 0;
    size_t completed = 0;

    while (completed < nprocs){
        int posBest = -1;
        for (size_t p = 0; p < nprocs; p++){
            if (!procTable[p].completed && procTable[p].arrive_time <= current_time){
                if (posBest == -1 || procTable[p].priority < procTable[posBest].priority){
                    posBest = (int)p;
                }
            }
        }

        if (posBest == -1){
            current_time++;
            continue;
        }

        Process *proc = &procTable[posBest];

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