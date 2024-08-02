#ifndef _NES_EXSOUND_H
#define _NES_EXSOUND_H

#include <kos.h>

int32 VRC6SoundRender();
void VRC6SoundWrite(uint32 address, uint8 value);
void VRC6SoundReset(void);
void VRC6SoundVolume(uint32 volume);

int32 OPLLSoundRender(void);
void OPLLSoundWrite(uint32 address, uint8 value);
void OPLLSoundReset(void);
void OPLLSoundVolume(uint32 volume);

int32 FDSSoundRender(void);
void FDSSoundWrite(uint32 address, uint8 value);
void FDSSoundReset(void);
void FDSSoundVolume(uint32 volume);
uint8 FDSSoundRead(uint32 address);
void FDSSoundWriteCurrent(uint32 address, uint8 value);

int32 PSGSoundRender(void);
void PSGSoundWrite(uint32 address, uint8 value);
void PSGSoundReset(void);
void PSGSoundVolume(uint32 volume);

int32 MMC5SoundRender(void);
void MMC5SoundWrite(uint32 address, uint8 value);
void MMC5SoundReset(void);
void MMC5SoundVolume(uint32 volume);

int32 N106SoundRender(void);
void N106SoundWrite(uint32 address, uint8 value);
void N106SoundReset(void);
void N106SoundVolume(uint32 volume);

void LogTableInitialize();

#endif