#include "input_recorder.h"

extern uint32 numEmulationFrames;
extern uint32 currentCRC32;

uint32 currentSample;
uint32 numSamples;
InputFrame_t frameInputs[MAXIMUM_INPUT_FRAMES];

void resetRecording() {
    currentSample = 0;
    numSamples = 0;
    memset(frameInputs, 0, sizeof(InputFrame_t) * MAXIMUM_INPUT_FRAMES);
}

void recordInput(InputFrame_t* input) {
    memcpy(&frameInputs[currentSample], input, sizeof(InputFrame_t));
    currentSample++;
    numSamples = currentSample;
}

void printRecording() {
    for (uint32 frameIndex = 0; frameIndex < numSamples; frameIndex++) {
        printf("Index [%lu]: Button [%u] Start [%lu] Duration [%u]\n", frameIndex, frameInputs[frameIndex].button, frameInputs[frameIndex].frameStart, frameInputs[frameIndex].frameDuration);
    }
}

void sortInputFrames() {
    printf("Sorting [%lu] frames\n", numSamples);
    if (numSamples > 0) {
        for (uint32 frameIndex = 0; frameIndex < numSamples - 1; frameIndex++) {
            uint32 minimumFrameStart = frameInputs[frameIndex].frameStart;
            uint32 minimumFrameIndex = frameIndex;
            for (uint32 nextIndex = frameIndex + 1; nextIndex < numSamples; nextIndex++) {
                if (frameInputs[nextIndex].frameStart < minimumFrameStart) {
                    minimumFrameStart = frameInputs[nextIndex].frameStart;
                    minimumFrameIndex = nextIndex;
                }
            }
            if (minimumFrameIndex != frameIndex) {
                InputFrame_t temp;
                memcpy(&temp, &frameInputs[frameIndex], sizeof(InputFrame_t));
                memcpy(&frameInputs[frameIndex], &frameInputs[minimumFrameIndex], sizeof(InputFrame_t));
                memcpy(&frameInputs[minimumFrameIndex], &temp, sizeof(InputFrame_t));
            }
        }
    }

    printRecording();
}

void uploadRecording() {
    printf("Uploading Input Recording to PC Host\n");
    sortInputFrames();

	char PCPath[256];

	// TODO: use a log time or something like that in the filename here
	snprintf(PCPath, 256, "/pc/Users/maslevin/Documents/Projects/numechanix/frnes/%08lX_recording.inp", currentCRC32);
	file_t PCFile = fs_open(PCPath, O_WRONLY);
	if (PCFile != -1) {
        fs_write(PCFile, frameInputs, currentSample * sizeof(InputFrame_t));
		fs_close(PCFile);
		printf("Closed file on PC\n");
	} else {
		printf("Error: Unable to Open File on PC Host\n");
	}    
}

bool loadRecording() {
    printf("Loading Input Recording from PC Host\n");
	char PCPath[256];
	// TODO: use a log time or something like that in the filename here
	snprintf(PCPath, 256, "/pc/Users/maslevin/Documents/Projects/numechanix/frnes/%08lX_recording.inp", currentCRC32);	
	file_t PCFile = fs_open(PCPath, O_RDONLY);
	if (PCFile != -1) {
        uint32 size = fs_read(PCFile, frameInputs, MAXIMUM_INPUT_FRAMES * sizeof(InputFrame_t));
        currentSample = 0;
        numSamples = size / sizeof(InputFrame_t);
        printf("Read Recording containing [%lu] samples\n", numSamples);
		fs_close(PCFile);

        printRecording();
		printf("Closed file on PC\n");
        return true;
	} else {
		printf("Error: Unable to Open File on PC Host\n");
	}

    return false;
}