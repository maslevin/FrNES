#ifndef _PROFILE_H
#define _PROFILE_H

#include <kos.h>

extern uint32 numEmulationFrames;

// #define PROFILE

#ifdef PROFILE
void resetProfiling(uint16 newProfilingMode, uint32 numFunctions);
void setProfilingFunctionName(uint32 functionIndex, const char* functionName);
void startProfiling(uint32 functionIndex);
void endProfiling(uint32 functionIndex);
void printProfilingReport();
#else
#define resetProfiling(...) (0)
#define setProfilingFunctionName(...) (0)
#define startProfiling(...) (0)
#define endProfiling(...) (0)
#define printProfilingReport() (0)
#endif

#endif