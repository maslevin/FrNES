/*===================================================================*/
/*                                                                   */
/*  GUI_VideoPage.c : GUI Page for Video Settings                    */
/*                    Interface, Variables, and Event Handling       */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#include "GUI_VideoPage.h"
#include "pNesX_System_Dc.h"
#include "pNesX_Utils.h"

uint16* opt_Stretch;
uint16* opt_Filter;
uint32* opt_ClipVars;

char ClipLX_Buffer[50];
char ClipRX_Buffer[50];
char ClipTX_Buffer[50];
char ClipBX_Buffer[50];

const int MAX_CLIP_PIXELS = 64;

//Video Options Text
char Options_Video[] = "Video Options";
char Options_Stretch_Checked[] = "<Check>Stretch Video,CBC_CHECKED";
char Options_Stretch_Unchecked[] = "<Check>Stretch Video,CBC_UNCHECKED";
char Options_Filter_Checked[] = "<Check>Bilinear Filter,CBC_CHECKED";
char Options_Filter_Unchecked[] = "<Check>Bilinear Filter,CBC_UNCHECKED";
char Options_Clip_Left[] = "Clip Left Pixels";
char Options_Clip_Right[] = "Clip Right Pixels";
char Options_Clip_Top[] = "Clip Top Pixels";
char Options_Clip_Bottom[] = "Clip Bottom Pixels";
char* Video_Options[6];
const int Num_Video_Options = 6;

void Allocate_Video_Options()
{
	opt_Stretch = malloc(sizeof(uint16));
	opt_Filter = malloc(sizeof(uint16));
	opt_ClipVars = malloc(4 * sizeof(uint32));
}

void Free_Video_Options()
{
	free(opt_Stretch);
	free(opt_Filter);
	free(opt_ClipVars);
}

void setup_video_options_screen()
{
	//Set Up Window Style Features
	mystyle.Border_Inside_Color = GUI_OutsideWindowColor; //		MakeRGB(8, 20, 10);
	mystyle.Border_Outside_Color = 0x8000;
	mystyle.Inside_Color = GUI_InsideWindowColor; //MakeRGB(8, 20, 32);
	mystyle.Left_Margin = 15;
	mystyle.Header_Text_Color = GUI_TextColor; //0x8000;
	mystyle.Text_Color = GUI_TextColor;
	mystyle.Max_Items = (mydata.height - (35 + mydata.Header_Font[0].height)) / (mydata.Item_Font[0].height);
	mystyle.Selected_Text_Color = GUI_TextColor;
	mystyle.Selected_Background_Color = GUI_SelectedTextColor; //MakeRGB(8, 18, 32);

	//Set Up Window Data Features
	mydata.x = 0;
	mydata.y = 0;
	mydata.width = 400;
	mydata.height = 460 - (2 * title_offset_y);
	mydata.Target_Width = 512;
	mydata.Header_Font = largefont;
	mydata.Item_Font = medfont;
	mydata.Header_Text = Options_Video;
	mydata.Data_Strings = Video_Options;
	mydata.Num_Strings = Num_Video_Options;
	mydata.Highlighted_Index = 0;
	mydata.Top_Index = 0;
	mydata.Target_Buffer = PVR_MainWindow_Offset;

	//Set Up Window Style Features
	helpstyle.Border_Inside_Color = GUI_OutsideWindowColor;
	helpstyle.Border_Outside_Color = 0x8000;
	helpstyle.Inside_Color = GUI_InsideWindowColor;
	helpstyle.Left_Margin = 15;
	helpstyle.Header_Text_Color = GUI_TextColor;
	helpstyle.Text_Color = GUI_TextColor;
	helpstyle.Max_Items = 10;
	helpstyle.Selected_Text_Color = GUI_TextColor;
	helpstyle.Selected_Background_Color = GUI_SelectedTextColor;//MakeRGB(31, 18, 8);

	//Set Up Window Data Features
	helpdata.x = 0;
	helpdata.y = 0;
	helpdata.width = 160;
	helpdata.height = 160;
	helpdata.Target_Width = 512;
	helpdata.Header_Font = smallfont;
	helpdata.Item_Font = smallfont;
	helpdata.Header_Text = " ";
	helpdata.Data_Strings = Options_Keys;
	helpdata.Num_Strings = Num_Options_Keys;
	helpdata.Highlighted_Index = Num_Options_Keys;
	helpdata.Top_Index = 0;
	helpdata.Target_Buffer = PVR_SmallWindow_Offset;
}

//Generates the Options GUI info from the variables in memory
void Generate_Video_Options_List()
{
	char tempbuffer[256];

	switch(*opt_Stretch)
	{
		case 0:
			Video_Options[0] = Options_Stretch_Unchecked;
			break;
		case 1:
			Video_Options[0] = Options_Stretch_Checked;
			break;
	}
	switch(*opt_Filter)
	{
		case 0:
			Video_Options[1] = Options_Filter_Unchecked;
			break;
		case 1:
			Video_Options[1] = Options_Filter_Checked;
			break;
	}

/*	pNesX_itoa(opt_ClipVars[0], ClipLX_Buffer);
	Video_Options[2] = ClipLX_Buffer;
	pNesX_itoa(opt_ClipVars[1], ClipRX_Buffer);
	Video_Options[3] = ClipRX_Buffer;
	pNesX_itoa(opt_ClipVars[2], ClipTX_Buffer);
	Video_Options[4] = ClipTX_Buffer;
	pNesX_itoa(opt_ClipVars[3], ClipBX_Buffer);
	Video_Options[5] = ClipBX_Buffer;*/

	pNesX_itoa(opt_ClipVars[0], tempbuffer);
	strcpy(ClipLX_Buffer, Options_Clip_Left);
	strcat(ClipLX_Buffer, ",");
	strcat(ClipLX_Buffer, tempbuffer);
	strcat(ClipLX_Buffer, "<RLAlign>");
	Video_Options[2] = ClipLX_Buffer;

	pNesX_itoa(opt_ClipVars[1], tempbuffer);
	strcpy(ClipRX_Buffer, Options_Clip_Right);
	strcat(ClipRX_Buffer, ",");
	strcat(ClipRX_Buffer, tempbuffer);
	strcat(ClipRX_Buffer, "<RLAlign>");
	Video_Options[3] = ClipRX_Buffer;

	pNesX_itoa(opt_ClipVars[2], tempbuffer);
	strcpy(ClipTX_Buffer, Options_Clip_Top);
	strcat(ClipTX_Buffer, ",");
	strcat(ClipTX_Buffer, tempbuffer);
	strcat(ClipTX_Buffer, "<RLAlign>");
	Video_Options[4] = ClipTX_Buffer;

	pNesX_itoa(opt_ClipVars[3], tempbuffer);
	strcpy(ClipBX_Buffer, Options_Clip_Bottom);
	strcat(ClipBX_Buffer, ",");
	strcat(ClipBX_Buffer, tempbuffer);
	strcat(ClipBX_Buffer, "<RLAlign>");
	Video_Options[5] = ClipBX_Buffer;
}

void Handle_Video_Interface(cont_cond_t my_cond)
{
	//Down Key Hit and Key is Ready to be hit
	if ((!(my_cond.buttons & CONT_DPAD_DOWN)) && (mydata.Highlighted_Index < Num_Video_Options) && (keyhit == 0))
	{
		mydata.Highlighted_Index++;
		if ((mydata.Highlighted_Index - mydata.Top_Index) >= mystyle.Max_Items)
			mydata.Top_Index++;
		keyhit = 1;
		isMainChanged = 1;
	}

	//Up Key Hit and Key is Ready to be hit
	if ((!(my_cond.buttons & CONT_DPAD_UP)) && (mydata.Highlighted_Index > 0) && (keyhit == 0))
	{
		mydata.Highlighted_Index--;
		if (mydata.Top_Index > mydata.Highlighted_Index)
			mydata.Top_Index--;
		keyhit = 1;
		isMainChanged = 1;
	}

	//Handle the toggle boxes
	if (!(my_cond.buttons & CONT_A) && (invalida == 0))
	{
		switch(mydata.Highlighted_Index)
		{
			case 0:
				*opt_Stretch = 1 - *opt_Stretch;
				Generate_Video_Options_List();
				isMainChanged = 1;
				invalida = 1;
				break;
			case 1:
				*opt_Filter = 1 - *opt_Filter;
				Generate_Video_Options_List();
				isMainChanged = 1;
				invalida = 1;
				break;
		}
	}

	//Handle the modify boxes
	if (!(my_cond.buttons & CONT_DPAD_LEFT) && (xkeyhit == 0))
	{
		switch(mydata.Highlighted_Index)
		{
			case 2:
				if (opt_ClipVars[0] > 0)
				{
					(opt_ClipVars[0]) -= 8;
					Generate_Video_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				break;
			case 3:
				if (opt_ClipVars[1] > 0)
				{
					(opt_ClipVars[1]) -= 8;
					Generate_Video_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				break;
			case 4:
				if (opt_ClipVars[2] > 0)
				{
					(opt_ClipVars[2]) -= 8;
					Generate_Video_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				break;
			case 5:
				if (opt_ClipVars[3] > 0)
				{
					(opt_ClipVars[3]) -= 8;
					Generate_Video_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				break;
		}
	}					

	if (!(my_cond.buttons & CONT_DPAD_RIGHT) && (xkeyhit == 0))
	{
		switch(mydata.Highlighted_Index)
		{
			case 2:
				if (opt_ClipVars[0] < MAX_CLIP_PIXELS)
				{
					(opt_ClipVars[0]) += 8;
					Generate_Video_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				break;
			case 3:
				if (opt_ClipVars[1] < MAX_CLIP_PIXELS)
				{
					(opt_ClipVars[1]) += 8;
					Generate_Video_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				break;
			case 4:
				if (opt_ClipVars[2] < MAX_CLIP_PIXELS)
				{
					(opt_ClipVars[2]) += 8;
					Generate_Video_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				break;
			case 5:
				if (opt_ClipVars[3] < MAX_CLIP_PIXELS)
				{
					(opt_ClipVars[3]) += 8;
					Generate_Video_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				break;
		}
	}					
	
	// Handle Return to Main Menu
	if (!(my_cond.buttons & CONT_B) && (keyhit == 0))
	{
		setup_main_menu();
		menuscreen = MENUNUM_MAIN;
		isMainChanged = 1;
		isSmallChanged = 1;
	}
}