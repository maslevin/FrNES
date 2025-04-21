#include "profile.h"

uint32 numEmulationFrames = 0;

#ifdef PROFILE
uint32 numProfilingFunctions = 0;
uint32 profilingMode = PMCR_PIPELINE_FREEZE_BY_ICACHE_MISS_MODE;
const char* PROFILING_MODE_STRINGS[] = {
    "No Mode (0x00)",
    "Operand Read Accesses", // 0x01
    "Operand Write Accesses", // 0x02
    "UTLB Misses", // 0x03
    "Operand Cache Read Misses",
    "Operand Cache Write Misses",
    "Instruction Fetches",
    "Instruction TLB Misses",
    "Instruction Cache Misses",
    "Operand Accesses",
    "Instruction Fetches",
    "On Chip RAM Operand Accesses",
    "No Mode (0x0C)", 
    "On Chip IO Accesses",
    "Operand Accesses",
    "Operand Cache Misses",
    "Branches Issued",
    "Branches Taken",
    "Subroutines Issued",
    "Instructions Issued",
    "Parallel Instructions Issued",
    "FPU Instructions Issued",
    "Interrupts",
    "NMIs",
    "TRAPA Instructions",
    "UBC A Matches",
    "UBC B Matches",
    "No Mode (0x1B)",
    "No Mode (0x1C)",
    "No Mode (0x1D)",
    "No Mode (0x1E)",
    "No Mode (0x1F)",
    "No Mode (0x20)",
    "Instruction Cache Fills",
    "Operand Cache Fills",
    "Elapsed Time",
    "Pipeline Freezes by Instruction Cache Misses",
    "Pipeline Freezes by Data Cache Misses",
    "No Mode",
    "Pipeline Freezes by Branch Mode",
    "Pipeline Freezes by CPU Register Mode",
    "Pipeline Freezes by FPU Mode"
};

typedef struct __attribute__ ((packed, aligned(4))) {
    char* functionName;
    uint64 startTime;
    uint64 nanoseconds;
    uint64 startPerfCounterValue;
    uint64 totalPerfCounterValue;
} ProfilingInformation_t;

ProfilingInformation_t* profilingInformation;
bool resetPerformanceCounter = false;

void resetProfiling(uint32 newProfilingMode, uint32 numFunctions) {
    profilingMode = newProfilingMode;

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

    perf_cntr_start(PRFC1, profilingMode, PMCR_COUNT_CPU_CYCLES);
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
    profilingInformation[functionIndex].startPerfCounterValue = perf_cntr_count(PRFC1);
}

void endProfiling(uint32 functionIndex) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
    profilingInformation[functionIndex].nanoseconds += (ts.tv_sec * 1000000000ULL + ts.tv_nsec) - profilingInformation[functionIndex].startTime;
    profilingInformation[functionIndex].totalPerfCounterValue += perf_cntr_count(PRFC1) - profilingInformation[functionIndex].startPerfCounterValue;
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
    printf("---------------- %s ---------------\n", PROFILING_MODE_STRINGS[profilingMode]);
    const char* itemString = PROFILING_MODE_STRINGS[profilingMode];
    if (profilingMode == 0x23) {
        itemString = "cycles";
    } else if (profilingMode > 0x23) {
        itemString = "lost cycles due to pipeline freeze";
    }
    for (uint32 i = 0; i < numProfilingFunctions; i++) {
        printf("%s: %.2f %s per frame\n", 
            profilingInformation[i].functionName,  
            (float)profilingInformation[i].totalPerfCounterValue / (float)numEmulationFrames,
            itemString);
    }
    printf("*****************************************************\n");
}
#endif