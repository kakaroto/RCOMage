#include "general.h"
uint8_t vag2wav (const char *fWav, int numChannels, int *vagLen, void **vagData);
int wav2vag (const char *fWav, uint32_t * len, void **vagData, const char *vagName);
