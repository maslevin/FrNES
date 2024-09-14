#ifndef _OPTIONS_H
#define _OPTIONS_H

#include <kos.h>

typedef struct __attribute__ ((packed, aligned(4))) ControllerSettings_s {
    bool analogEnabled;
    uint8 selectKey;
    uint8 aKey;
    uint8 bKey;
} ControllerSettings_t;

#define DEFAULT_SOUND 1
#define DEFAULT_FRAMESKIP 0
#define DEFAULT_ANALOG false
#define DEFAULT_SELECT DC_CONTROLLER_BUTTON_LTRIG
#define DEFAULT_AKEY DC_CONTROLLER_BUTTON_A
#define DEFAULT_BKEY DC_CONTROLLER_BUTTON_X
#define DEFAULT_STRETCH 1
#define DEFAULT_FILTER 1
#define DEFAULT_PROFILE 1
#define DEFAULT_VMUPORT 0
#define DEFAULT_SRAM 1
#define DEFAULT_AUTOFRAMESKIP 0
#define DEFAULT_SHOWFRAMERATE 0
#define DEFAULT_CLIP_LEFT 0
#define DEFAULT_CLIP_RIGHT 0
#define DEFAULT_CLIP_TOP 8
#define DEFAULT_CLIP_BOTTOM 8

#define DC_CONTROLLER_BUTTON_A     0
#define DC_CONTROLLER_BUTTON_B     1
#define DC_CONTROLLER_BUTTON_X     2
#define DC_CONTROLLER_BUTTON_Y     3
#define DC_CONTROLLER_BUTTON_LTRIG 4

extern ControllerSettings_t controllerSettings[];

extern uint32 GUI_BGColor;
extern uint32 GUI_TextColor;
extern uint32 GUI_SelectedTextColor;
extern uint32 GUI_InsideWindowColor;
extern uint32 GUI_OutsideWindowColor;

//Options Variables
extern uint16 opt_SoundEnabled;
extern uint16 opt_FrameSkip;
extern uint16 opt_AutoFrameSkip;
extern uint16 opt_ShowFrameRate;
extern int16 opt_VMUPort;
extern uint16 opt_SRAM;

#endif