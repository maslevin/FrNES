#include "profile.h"

uint32 numEmulationFrames = 0;

#ifdef PROFILE
uint32 numProfilingFunctions = 0;
uint64* profilingFunctionsNanoseconds = NULL;
char** profilingFunctionsNames = NULL;

uint64* profilingFunctionsStartTimes = NULL;

void resetProfiling() {
    numEmulationFrames = 0;
    if (profilingFunctionsNanoseconds != NULL) {
        free(profilingFunctionsNanoseconds);
    }
    if (profilingFunctionsStartTimes != NULL) {
        free(profilingFunctionsStartTimes);
    }
    if (profilingFunctionsNames != NULL) {
        for (uint32 i = 0; i < numProfilingFunctions; i++) {
            free(profilingFunctionsNames[i]);
        }
        free(profilingFunctionsNames);
    }

    profilingFunctionsNanoseconds = (uint64*)malloc(sizeof(uint64) * numProfilingFunctions);
    profilingFunctionsStartTimes = (uint64*)malloc(sizeof(uint64) * numProfilingFunctions);
    profilingFunctionsNames = (char**)malloc(sizeof(char*) * numProfilingFunctions);

    memset(profilingFunctionsNanoseconds, 0, sizeof(uint64) * numProfilingFunctions);
    memset(profilingFunctionsStartTimes, 0, sizeof(uint64) * numProfilingFunctions);
    memset(profilingFunctionsNames, 0, sizeof(char*) * numProfilingFunctions);

    numEmulationFrames = 0;
}

void setMaximumProfilingFunctions(uint32 maximumFunctions) {
    numProfilingFunctions = maximumFunctions;
}

void setProfilingFunctionName(uint32 functionIndex, const char* functionName) {
    char* functionCopy = malloc(strlen(functionName) + 1);
    strcpy(functionCopy, functionName);
    profilingFunctionsNames[functionIndex] = functionCopy;
}

void startProfiling(uint32 functionIndex) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
    profilingFunctionsStartTimes[functionIndex] = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void endProfiling(uint32 functionIndex) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
    profilingFunctionsNanoseconds[functionIndex] += (ts.tv_sec * 1000000000ULL + ts.tv_nsec) - profilingFunctionsStartTimes[functionIndex];
}

void printProfilingReport() {
    printf("****************** PROFILING REPORT *****************\n");
    printf("Over [%lu] Frames\n", numEmulationFrames);
    for (uint32 i = 0; i < numProfilingFunctions; i++) {
        float nanosecondsPerFrame = (float)profilingFunctionsNanoseconds[i] / (float)numEmulationFrames;
        printf("%s: %.2f nanoseconds per frame\n", profilingFunctionsNames[i], nanosecondsPerFrame);
    }
}
#endif