
#ifndef __RCOXML_H__
#define __RCOXML_H__

#include "general.h"
#include "rcomain.h"

Bool write_xml(rRCOFile* rco, FILE* fp, char* textDir, Bool textXmlOut, int sndDumped, Bool vsmxConv);
rRCOFile* read_xml(char* fn);


#define RCOXML_TABLE_2ND_DIM 20
typedef char ((*RcoTableMap)[RCOXML_TABLE_2ND_DIM]); // doesn't make sense...  I want a pointer to an array, not an array of pointers...
extern RcoTableMap RCOXML_TABLE_DATA_COMPRESSION;

extern RcoTableMap RCOXML_TABLE_TEXT_LANG;
extern RcoTableMap RCOXML_TABLE_TEXT_FMT;
extern RcoTableMap RCOXML_TABLE_IMG_FMT;
extern RcoTableMap RCOXML_TABLE_MODEL_FMT;
extern RcoTableMap RCOXML_TABLE_SOUND_FMT;
extern RcoTableMap RCOXML_TABLE_REFTYPE;


extern RcoTagMap RCOXML_TABLE_TAGS;

extern uint RCOXML_TABLE_TAGS_NUM;

extern RcoTableMap RCOXML_TABLE_NAMES;




void rcoxml_int_to_text(uint in, const RcoTableMap map, char* out);
Bool rcoxml_text_to_int(char* s, const RcoTableMap map, uint* out);


#endif
