/*===================================================================*/
/*                                                                   */
/*  GUI_VideoPage.h : GUI Page for Video Settings                    */
/*                    Interface, Variables, and Event Handling       */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#ifndef GUI_FILEBROWSER
#define GUI_FILEBROWSER

#include <kos.h>
#include "TextWindow.h"
#include "pNesX_System_DC.h"
#include "options.h"

#include "GUI_MainMenu.h"

//Function Definitions
void setup_file_browser_screen();
void Handle_File_Browser_Interface(cont_state_t* my_state);

#endif