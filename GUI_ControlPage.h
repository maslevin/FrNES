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
void Allocate_Control_Options();
void Free_Control_Options();

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

//Options Variables
extern uint16* opt_P1AnalogEnabled;
extern uint16* opt_P2AnalogEnabled;
extern uint16* opt_P1SelectKey;
extern uint16* opt_P1AKey;
extern uint16* opt_P1BKey;
extern uint16* opt_P2SelectKey;
extern uint16* opt_P2AKey;
extern uint16* opt_P2BKey;

#endif