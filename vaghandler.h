#include "general.h"
Bool vag2wav(const char* fWav, int numChannels, int* vagLen, void** vagData);
int wav2vag(const char* fWav, uint* len, void** vagData, const char* vagName);
