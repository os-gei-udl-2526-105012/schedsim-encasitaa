#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "process.h"
#include "queue.h"
#include "scheduler.h"

int num_algorithms() {
  return sizeof(algorithmsNames) / sizeof(char *);
}

int num_modalities() {
  return sizeof(modalitiesNames) / sizeof(char *);
}

size_t initFromCSVFile(char* filename, Process** procTable){
    FILE* f = fopen(filename,"r");
    
    size_t procTableSize = 10;
    
    *procTable = malloc(procTableSize * sizeof(Process));
    Process * _procTable = *procTable;

    if(f == NULL){
      perror("initFromCSVFile():::Error Opening File:::");   
      exit(1);             
    }

    char* line = NULL;
    size_t buffer_size = 0;
    size_t nprocs= 0;
    while( getline(&line,&buffer_size,f)!=-1){
        if(line != NULL){
            Process p = initProcessFromTokens(line,";");

            if (nprocs==procTableSize-1){
                procTableSize=procTableSize+procTableSize;
                _procTable=realloc(_procTable, procTableSize * sizeof(Process));
            }

            _procTable[nprocs]=p;

            nprocs++;
        }
    }
   free(line);
   fclose(f);
   return nprocs;
}

size_t getTotalCPU(Process *procTable, size_t nprocs){
    size_t total=0;
    for (int p=0; p<nprocs; p++ ){
        total += (size_t) procTable[p].burst;
    }
    return total;
}

int getCurrentBurst(Process* proc, int current_time){
    int burst = 0;
    for(int t=0; t<current_time; t++){
        if(proc->lifecycle[t] == Running){
            burst++;
        }
    }
    return burst;
}

int run_dispatcher(Process *procTable, size_t nprocs, int algorithm, int modality, int quantum){

    qsort(procTable,nprocs,sizeof(Process),compareArrival);

    init_queue(); //inicialitzar la cua per RR

    //calcular durada de la simulació suficient
    size_t totalCPU = getTotalCPU(procTable, nprocs) +1;
    int max_arrival = 0;
    for(size_t i = 0; i < nprocs; i++){
        if(procTable[i].arrive_time > max_arrival){ 
            max_arrival = procTable[i].arrive_time;
        }
    }
    size_t duration = totalCPU + (size_t)max_arrival + 1u;

    //arrays de control 
    int *remaining = malloc(nprocs * sizeof(int));  //CPU restant
    int *started   = malloc(nprocs * sizeof(int));  //per response_time
    if(!remaining || !started){ perror("malloc"); exit(1); }

    //inicialitzar processos
    for(size_t p = 0; p < nprocs; p++){
        remaining[p] = procTable[p].burst;
        started[p]   = 0;

        procTable[p].waiting_time = 0;
        procTable[p].return_time = 0;
        procTable[p].response_time = 0;
        procTable[p].completed = false;
    
        procTable[p].lifecycle = malloc(duration * sizeof(int));
        if(procTable[p].lifecycle == NULL){
            perror("malloc lifecycle");
            exit(1);
        }
        for(size_t t = 0; t < duration; t++) {
            procTable[p].lifecycle[t] = -1;
        }
    }
    
    //variables del planificador
    int time = 0;
    int finished = 0;
    int current = -1; //procés seleccionat
    int rr_counter = 0;	//comptador RR


    //BUCLE PRINCIPAL 

    while(finished < (int)nprocs)
    {
        //processos que arriben
        for(size_t p = 0; p < nprocs; p++){
            if(procTable[p].arrive_time == time){
                enqueue(&procTable[p]); //entren a la cua (serveix per RR)
            }  
        }
        int next = -1;


        //SELECCIÓ D'ALGORISME

        if(algorithm == FCFS){
            if(current == -1 || modality == PREEMPTIVE){
                //buscar primer procés "ready"
                for(size_t p = 0; p < nprocs; p++){
                    if(!procTable[p].completed && procTable[p].arrive_time <= time && remaining[p] > 0){
                        next = (int)p;
                        break;
                    }
                }
            } 
            else {
                next = current;
            }
        }

        else if(algorithm == SJF){
            int best = -1;
            for(size_t p = 0; p < nprocs; p++){
                if(!procTable[p].completed && procTable[p].arrive_time <= time && remaining[p] > 0){
                    if(best == -1 || remaining[p] < remaining[best]){
                        best = (int)p;
                    }
                }
            }
            if(modality == NONPREEMPTIVE && current != -1){
                next = current;  //no canvia fins acabar
            }
            else{
                next = best;
            }
        }

        else if(algorithm == PRIORITIES){
            int best = -1;
            for(size_t p = 0; p < nprocs; p++){
                if(!procTable[p].completed && procTable[p].arrive_time <= time && remaining[p] > 0){
                    if(best == -1 || procTable[p].priority < procTable[best].priority){
                        best =(int)p;
                    }
                }
            }
            if(modality == NONPREEMPTIVE && current != -1){
                next = current;
            }
            else{
                next = best;
            }
        }

        else if(algorithm == RR){
            if(current == -1 || rr_counter >= quantum){
                if(get_queue_size() > 0){
                    Process *p = dequeue();
                    if(p == NULL){ //cua buida o error
                        next = -1;
                    }
                    else{
                        //calculem l'índex basat en el punter retornat
                        ptrdiff_t idx = p - procTable;
                        if(idx >= 0 && (size_t)idx < nprocs){
                            next = (int)idx;
                            //tornar a encolar el mateix punter al final
                            enqueue(p);
                        }
                        else{ 
                            //punter invàlid dins la cua
                            next = -1;
                        }

                    }
                    rr_counter = 0;
                }
            } 
            else{
                next = current;
            }
        }

        //EXECUCIÓ

        if(next == -1){
            time++;
            continue;
        }

        //marcar primer cop d’execució
        if(started[next] == 0){
            procTable[next].response_time = time - procTable[next].arrive_time;
            started[next] = 1;
        }

        //marcar estat
        for(size_t k = 0; k < nprocs; k++){
            if(k == (size_t)next){
                procTable[k].lifecycle[time] = Running;
            }
            else if(!procTable[k].completed && procTable[k].arrive_time <= time){
                procTable[k].lifecycle[time] = Ready;
            }
        }

        //execució real 
        remaining[next]--;
        rr_counter++;
        current = next;

        //procés acabat
        if(remaining[next] == 0){
            procTable[next].completed = true;
            procTable[next].return_time = time - procTable[next].arrive_time + 1;
            finished++;
            rr_counter = 0;

            //marquem estat de finalitzat
            if((size_t)(time + 1) < duration){
                procTable[next].lifecycle[time+1] = Finished;
            }
        }
        time++;
    }

    //calcular waiting_time comptant Ready en lifecycle
    for(size_t p = 0; p < nprocs; p++){
        size_t w = 0;
        for(size_t t = 0; t < duration; t++){
            if(procTable[p].lifecycle[t] == Ready) w++;
        }
        procTable[p].waiting_time = (int)w;
    }

    //MOSTRAR RESULTATS 

    printSimulation(nprocs, procTable, duration);
    printMetrics((size_t)(time-1), nprocs, procTable);

    //NETEJA 

    free(remaining);
    free(started);

    //alliberem lifecycle per evitar acumulació de memòria
    for(size_t p = 0; p < nprocs; p++){
        if(procTable[p].lifecycle){
            free(procTable[p].lifecycle);
            procTable[p].lifecycle = NULL;
        }
    }

    cleanQueue();

    return EXIT_SUCCESS;
}


void printSimulation(size_t nprocs, Process *procTable, size_t duration){

    printf("%14s","== SIMULATION ");
    for (int t=0; t<duration; t++ ){
        printf("%5s","=====");
    }
    printf("\n");

    printf ("|%4s", "name");
    for(int t=0; t<duration; t++){
        printf ("|%2d", t);
    }
    printf ("|\n");

    for (int p=0; p<nprocs; p++ ){
        Process current = procTable[p];
            printf ("|%4s", current.name);
            for(int t=0; t<duration; t++){
                printf("|%2s",  (current.lifecycle[t]==Running ? "E" : 
                        current.lifecycle[t]==Bloqued ? "B" :   
                        current.lifecycle[t]==Finished ? "F" : " "));
            }
            printf ("|\n");
        
    }


}

void printMetrics(size_t simulationCPUTime, size_t nprocs, Process *procTable ){

    printf("%-14s","== METRICS ");
    for (int t=0; t<simulationCPUTime+1; t++ ){
        printf("%5s","=====");
    }
    printf("\n");

    printf("= Duration: %ld\n", simulationCPUTime );
    printf("= Processes: %ld\n", nprocs );

    size_t baselineCPUTime = getTotalCPU(procTable, nprocs);
    double throughput = (double) nprocs / (double) simulationCPUTime;
    double cpu_usage = (double) simulationCPUTime / (double) baselineCPUTime;

    printf("= CPU (Usage): %lf\n", cpu_usage*100 );
    printf("= Throughput: %lf\n", throughput*100 );

    double averageWaitingTime = 0;
    double averageResponseTime = 0;
    double averageReturnTime = 0;
    double averageReturnTimeN = 0;

    for (int p=0; p<nprocs; p++ ){
            averageWaitingTime += procTable[p].waiting_time;
            averageResponseTime += procTable[p].response_time;
            averageReturnTime += procTable[p].return_time;
            averageReturnTimeN += procTable[p].return_time / (double) procTable[p].burst;
    }


    printf("= averageWaitingTime: %lf\n", (averageWaitingTime/(double) nprocs) );
    printf("= averageResponseTime: %lf\n", (averageResponseTime/(double) nprocs) );
    printf("= averageReturnTimeN: %lf\n", (averageReturnTimeN/(double) nprocs) );
    printf("= averageReturnTime: %lf\n", (averageReturnTime/(double) nprocs) );

}