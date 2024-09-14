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
#include "pNesX_System_DC.h"

#include "GUI_MainMenu.h"

//Function Definitions
void setup_video_options_screen();
void Generate_Video_Options_List();
void Handle_Video_Interface(cont_state_t* my_state);

#endif