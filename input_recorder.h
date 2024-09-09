#ifndef _INPUT_RECORDER_H
#define _INPUT_RECORDER_H

#include <kos.h>

#define MAXIMUM_INPUT_FRAMES 131072

#define CONTROLLER_BUTTON_A 0
#define CONTROLLER_BUTTON_B 1
#define CONTROLLER_BUTTON_SELECT 2
#define CONTROLLER_BUTTON_START 3
#define CONTROLLER_BUTTON_UP 4
#define CONTROLLER_BUTTON_DOWN 5
#define CONTROLLER_BUTTON_LEFT 6
#define CONTROLLER_BUTTON_RIGHT 7

typedef struct __attribute__ ((packed, aligned(4))) InputFrame_s {
    uint32 frameStart;
    uint16 frameDuration;
    uint8 controller;    
    uint8 button;
} InputFrame_t;

extern uint32 currentSample;
extern uint32 numSamples;
extern InputFrame_t frameInputs[MAXIMUM_INPUT_FRAMES];

void resetRecording();
void recordInput(InputFrame_t* input);
void uploadRecording();
bool loadRecording();

#endif