
#ifndef __CONFIGSCAN_H__
#define __CONFIGSCAN_H__
#include "general.h"

extern char* configDir;

void configLoadTagmap(void);
void configLoadMiscmap(void);
void configLoadObjmap(Bool ps3);
void configLoadAnimmap(Bool ps3);

#endif
