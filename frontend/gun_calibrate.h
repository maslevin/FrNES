#ifndef _GUN_CALIBRATE_H
#define _GUN_CALIBRATE_H

#include <stddef.h>

/**
 * Calibrates the light gun in Dreamcast port 'port' (0-3): four targets on a white screen, shot in turn, give the
 * options' gun calibration. 0 when it was set; 'result' says what happened, for the status line
 */
int gun_calibrate(int port, char* result, size_t size);

#endif
