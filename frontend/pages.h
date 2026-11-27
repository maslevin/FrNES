/*===================================================================*/
/*                                                                   */
/*  pages.h : The menu pages                                         */
/*                                                                   */
/*===================================================================*/

#ifndef __PAGES_H
#define __PAGES_H

#include "menu.h"

extern Menu_Page main_menu_page;

// the main menu with the SD card's page or without (the adapter there or not)
void pages_sd_present(int present);

#endif
