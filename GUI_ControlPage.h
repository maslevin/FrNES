/*===================================================================*/
/*                                                                   */
/*  GUI_ControlPage.h : GUI Page for Control Settings                */
/*                      Interface, Variables, and Event Handling     */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#ifndef GUI_CONTROLPAGE
#define GUI_CONTROLPAGE

#include <kos.h>
#include "TextWindow.h"

#include "GUI_MainMenu.h"

//Function Definitions
void setup_control_options_screen();
void Generate_Control_Options_List();
void Handle_Control_Interface(cont_state_t* my_state);

//External Variables Required
extern Window_Style mystyle;
extern Window_Data mydata;
extern Window_Style helpstyle;
extern Window_Data helpdata;

extern int keyhit;
extern int invalida;
extern int xkeyhit;
extern int menuscreen;
extern const int MAX_CLIP_PIXELS;
extern const int title_offset_y;
extern const int Max_Frameskip;

extern char* Main_Keys[];
extern char* Options_Keys[];
extern const int Num_Options_Keys;

typedef struct __attribute__ ((packed, aligned(4))) ControllerSettings_s {
    bool analogEnabled;
    uint8 selectKey;
    uint8 aKey;
    uint8 bKey;
} ControllerSettings_t;

#define DC_CONTROLLER_BUTTON_A     0
#define DC_CONTROLLER_BUTTON_B     1
#define DC_CONTROLLER_BUTTON_X     2
#define DC_CONTROLLER_BUTTON_Y     3
#define DC_CONTROLLER_BUTTON_LTRIG 4

extern ControllerSettings_t controllerSettings[];

#endif