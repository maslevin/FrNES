#include "profile.h"

uint32 numEmulationFrames = 0;

#ifdef PROFILE
uint32 numProfilingFunctions = 0;

typedef struct ProfilingInformation {
    char* functionName;
    uint64 startTime;
    uint64 nanoseconds;
    uint64 startOperationCacheMisses;
    uint64 operandCacheMisses;
} ProfilingInformation_t;

ProfilingInformation_t* profilingInformation;
bool resetPerformanceCounter = false;

void resetProfiling(uint32 numFunctions) {
    if (profilingInformation) {
        for (uint32 i = 0; i < numProfilingFunctions; i++) {
            free(profilingInformation[i].functionName);
        }
        free(profilingInformation);
    }

    profilingInformation = (ProfilingInformation_t*)malloc(sizeof(ProfilingInformation_t) * numFunctions);
    memset(profilingInformation, 0, sizeof(ProfilingInformation_t) * numFunctions);
    numProfilingFunctions = numFunctions;

    if (resetPerformanceCounter) {
        perf_cntr_stop(PRFC1);
        perf_cntr_clear(PRFC1);
    }

    //TODO: change profiling statistic depending on what the user or test requires

    perf_cntr_start(PRFC1, PMCR_OPERAND_CACHE_MISS_MODE, PMCR_COUNT_CPU_CYCLES);
    resetPerformanceCounter = true;

    numEmulationFrames = 0;
}

void setProfilingFunctionName(uint32 functionIndex, const char* functionName) {
    char* functionCopy = malloc(strlen(functionName) + 1);
    strcpy(functionCopy, functionName);
    profilingInformation[functionIndex].functionName = functionCopy;
}

void startProfiling(uint32 functionIndex) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
    profilingInformation[functionIndex].startTime = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    profilingInformation[functionIndex].startOperationCacheMisses = perf_cntr_count(PRFC1);
}

void endProfiling(uint32 functionIndex) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
    profilingInformation[functionIndex].nanoseconds += (ts.tv_sec * 1000000000ULL + ts.tv_nsec) - profilingInformation[functionIndex].startTime;
    profilingInformation[functionIndex].operandCacheMisses += perf_cntr_count(PRFC1) - profilingInformation[functionIndex].startOperationCacheMisses;
}

void printProfilingReport() {
    printf("****************** PROFILING REPORT *****************\n");
    printf("Over [%lu] Frames\n", numEmulationFrames);
    printf("------------------ Execution Time -------------------\n");
    for (uint32 i = 0; i < numProfilingFunctions; i++) {
        printf("%s: %.2f nanoseconds per frame\n", 
            profilingInformation[i].functionName, 
            (float)profilingInformation[i].nanoseconds / (float)numEmulationFrames);
    }
    printf("---------------- Operand Cache Misses ---------------\n");    
    for (uint32 i = 0; i < numProfilingFunctions; i++) {
        printf("%s: %.2f operand cache misses per frame\n", 
            profilingInformation[i].functionName,  
            (float)profilingInformation[i].operandCacheMisses / (float)numEmulationFrames);
    }
    printf("*****************************************************\n");
}
#endif