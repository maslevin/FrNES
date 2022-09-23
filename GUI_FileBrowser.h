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

#include "GUI_MainMenu.h"

//Function Definitions
void setup_file_browser_screen();
void Handle_File_Browser_Interface(cont_state_t* my_state);

//External Variables Required
extern Window_Style mystyle;
extern Window_Data mydata;
extern Window_Style helpstyle;
extern Window_Data helpdata;

extern int keyhit;
extern int invalida;
extern int xkeyhit;
extern int disable_trigs;
extern int menuscreen;
extern const int MAX_CLIP_PIXELS;
extern const int title_offset_y;

extern unsigned char* ROM;
extern unsigned char* VROM;
extern uint32 SRAM_Enabled;

#endif