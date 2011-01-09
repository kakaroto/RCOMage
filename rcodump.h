
#ifndef __RCODUMP_H__
#define __RCODUMP_H__

typedef Bool(*OutputDumpFunc)(char*,void*,rRCOEntry*,void*);

typedef struct {
	char* cmd;
	char* ext;
	char* extFlags;
} RcoDumpGimconvOpts;

Bool dump_resource(char* dest, rRCOEntry* entry, OutputDumpFunc outputfunc, void* outputfuncArg);
void dump_resources(char* labels, rRCOEntry* parent, const RcoTableMap extMap, char* pathPrefix, void* outputFilterArg);
void dump_text_resources(char* labels, rRCOEntry* parent, Bool writeHeader, char* pathPrefix, Bool bWriteXML);
Bool dump_output_data(char* dest, void* buf, rRCOEntry* entry, void* arg);
Bool dump_output_vsmxdec(char* dest, void* buf, rRCOEntry* entry, void* arg);

void compile_gimconv_map(rRCOFile* rco, rRCOEntry* entry, void* arg);
void compile_vagconv_map(rRCOFile* rco, rRCOEntry* entry, void* arg);
void compile_vsmxconv_map(rRCOFile* rco, rRCOEntry* entry, void* arg);
void compile_wavcheck_map(rRCOFile* rco, rRCOEntry* entry, void* arg);

#endif
