#ifndef _OPTIONS_H
#define _OPTIONS_H

#include <kos.h>

typedef struct __attribute__ ((packed, aligned(4))) ControllerSettings_s {
    bool analogEnabled;
    uint8 selectKey;
    uint8 aKey;
    uint8 bKey;
} ControllerSettings_t;

typedef struct __attribute__ ((packed, aligned(4))) Options_s {
    uint32 GUI_BGColor;
    uint32 GUI_TextColor;
    uint32 GUI_SelectedTextColor;
    uint32 GUI_InsideWindowColor;
    uint32 GUI_OutsideWindowColor;
    uint16 opt_SoundEnabled;
    uint16 opt_FrameSkip;
    uint16 opt_AutoFrameSkip;
    uint16 opt_ShowFrameRate;
    int16 opt_VMUPort;
    uint16 opt_SRAM;
    uint16 opt_Stretch;
    uint16 opt_Filter;
    uint8 opt_ClipVars[4];
    ControllerSettings_t controllerSettings[4];    
} Options_t;

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

extern Options_t options;

bool load_options_from_VMU();
bool save_options_to_VMU();
bool delete_options_from_VMU();

#endif