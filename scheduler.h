#ifndef __SCHEDULER__
#define __SCHEDULER__

enum algorithms{FCFS, SJF, RR, PRIORITIES}; 
static const char * const algorithmsNames[] = {
	[FCFS] = "fcfs",
	[SJF] = "sjf",
    [RR] = "rr",
    [PRIORITIES] = "priorities",
};

enum modalities{PREEMPTIVE, NONPREEMPTIVE}; 
static const char * const modalitiesNames[] = {
	[PREEMPTIVE] = "preemptive",
	[NONPREEMPTIVE] = "nonpreemptive",
};

int num_algorithms(void);
int num_modalities(void);

size_t initFromCSVFile(char* filename, Process** procTable);

int run_dispatcher(Process *procTable, size_t nprocs, int algorithm, int modality, int quantum);
void printMetrics(size_t simulationCPUTime, size_t nprocs, Process *procTable );
void printSimulation(size_t nprocs, Process *procTable, size_t duration);
int getCurrentBurst(Process* proc, int current_time);
size_t getTotalCPU(Process *procTable, size_t nprocs);

// Prototips de les funcions auxiliars
int fcfs(Process *procTable, size_t nprocs);
int sjf(Process *procTable, size_t nprocs, int preemptive);
int rr(Process *procTable, size_t nprocs, int quantum);
int priority(Process *procTable, size_t nprocs, int preemptive);
#endif
