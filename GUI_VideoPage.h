/*===================================================================*/
/*                                                                   */
/*  GUI_VideoPage.h : GUI Page for Video Settings                    */
/*                    Interface, Variables, and Event Handling       */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#ifndef GUI_VIDEOPAGE
#define GUI_VIDEOPAGE

#include <kos.h>
#include "TextWindow.h"

#include "GUI_MainMenu.h"

//Function Definitions
void setup_video_options_screen();
void Generate_Video_Options_List();
void Handle_Video_Interface(cont_state_t* my_state);

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

extern char* Main_Keys[];
extern char* Options_Keys[];
extern const int Num_Options_Keys;

//Interface for Local Variables
extern char Options_Video[];
extern char* Video_Options[];
extern char ClipLX_Buffer[];
extern char ClipRX_Buffer[];
extern char ClipBX_Buffer[];
extern char ClipTX_Buffer[];

extern char Options_Filter_Checked[];
extern char Options_Filter_Unchecked[];
extern char Options_Stretch_Checked[];
extern char Options_Stretch_Unchecked[];
extern char Options_Clip_Left[];
extern char Options_Clip_Right[];
extern char Options_Clip_Top[];
extern char Options_Clip_Bottom[];
extern const int Num_Video_Options;

extern uint16 opt_Stretch;
extern uint16 opt_Filter;
extern uint8 opt_ClipVars[];

#endif