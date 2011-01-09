
#ifndef __CONFIGSCAN_H__
#define __CONFIGSCAN_H__
#include "general.h"

extern char *configDir;

void configLoadTagmap (void);
void configLoadMiscmap (void);
void configLoadObjmap (uint8_t ps3);
void configLoadAnimmap (uint8_t ps3);

#endif
