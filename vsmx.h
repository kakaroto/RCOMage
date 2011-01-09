
#ifndef __VSMX_H__
#define __VSMX_H__

typedef wchar_t wchar;

#define VSMX_SIGNATURE 0x584D5356 // "VSMX"
#define VSMX_VERSION 0x00010000
PACK_STRUCT(VSMXHeader, {
	uint32 sig;
	uint32 ver;
	
	uint32 codeOffset;
	uint32 codeLength;
	
	uint32 textOffset;
	uint32 textLength;
	uint32 textEntries;
	
	uint32 propOffset;
	uint32 propLength;
	uint32 propEntries;
	
	uint32 namesOffset;
	uint32 namesLength;
	uint32 namesEntries;
});

PACK_STRUCT(VSMXGroup, {
	uint32 id;
        union {
          uint32 u32;
          float  f;
        } val;
});

typedef struct {
	VSMXGroup* code;
	wchar *text, *prop;
	char *names;
	wchar **pText, **pProp;
	char **pNames;
	
	uint codeGroups, numText, numProp, numNames, lenText, lenProp, lenNames;
} VsmxMem;


VsmxMem* readVSMX(FILE* fp);
VsmxMem* readVSMXMem(const void *in);
void writeVSMX(FILE* fp, VsmxMem* in);
void* writeVSMXMem(unsigned int* len, VsmxMem* in);
int VsmxDecode(VsmxMem* in, FILE* out);
VsmxMem* VsmxEncode(FILE* in);
int VsmxDecompile(VsmxMem* in, FILE* out);
void freeVsmxMem(VsmxMem* vm);

#endif
