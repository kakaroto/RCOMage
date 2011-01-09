
#ifndef __RCOFILE_H__
#define __RCOFILE_H__

#include "general.h"

#define RCO_NULL_PTR 0xFFFFFFFF

#define RCO_TABLE_MAIN 1
#define RCO_TABLE_VSMX 2
#define RCO_TABLE_TEXT 3
#define RCO_TABLE_IMG 4
#define RCO_TABLE_MODEL 5
#define RCO_TABLE_SOUND 6
#define RCO_TABLE_FONT 7
#define RCO_TABLE_OBJ 8
#define RCO_TABLE_ANIM 9

#define RCO_SIGNATURE 0x46525000 //.PRF (PSP Resource File?)
PACK_STRUCT(PRFHeader, {
	uint32 signature;  // RCO_SIGNATURE
	uint32 version;
				    // 0x70 - UMD RCOs (video?), FW1.00
				    // 0x71 - UMD RCOs (audio?), FW1.50, FW2.50
				    // 0x90 - FW2.60
				    // 0x95 - FW2.70, FW2.71
				    // 0x96 - FW2.80, FW2.82, FW3.00, FW3.03, FW3.10, FW3.30, FW3.40
				    // 0x100 - FW3.50, FW3.52, FW5.00, FW5.55
	uint32 null;
	uint32 compression; // upper nibble = compression, lower nibble: 0=flash0 RCOs, 1=UMD RCOs????
	/*
	#define RCO_COMPRESSION_NONE 0x00
	// entries for 0x01 ??
	#define RCO_COMPRESSION_ZLIB 0x10
	#define RCO_COMPRESSION_RLZ 0x20
	*/
	
	// main table pointers
	uint32 pMainTable;		// type 1
	uint32 pVSMXTable;		// type 2
	uint32 pTextTable;		// type 3
	uint32 pSoundTable;		// type 5
	uint32 pModelTable;		// type 6
	uint32 pImgTable;		// type 4
	uint32 pUnknown;	// always 0xFFFFFFFF
	uint32 pFontTable;		// type 7
	uint32 pObjTable;		// type 8
	uint32 pAnimTable;		// type 9
	
	// text stuff
	uint32 pTextData;	// NOTE: this may == pLabelData if lTextData == 0
	uint32 lTextData;
	uint32 pLabelData;
	uint32 lLabelData;
	uint32 pEventData;
	uint32 lEventData;
	
	// pointer data
	uint32 pTextPtrs;
	uint32 lTextPtrs;
	uint32 pImgPtrs;
	uint32 lImgPtrs;
	uint32 pModelPtrs;
	uint32 lModelPtrs;
	uint32 pSoundPtrs;
	uint32 lSoundPtrs;
	uint32 pObjPtrs;
	uint32 lObjPtrs;
	uint32 pAnimPtrs;
	uint32 lAnimPtrs;
	
	// attached data
	uint32 pImgData;
	uint32 lImgData;
	uint32 pSoundData;
	uint32 lSoundData;
	uint32 pModelData;
	uint32 lModelData;
	
	// always 0xFFFFFFFF
	uint32 unknown[3];
});

PACK_STRUCT(RCOEntry, {
	//uint8 type; // main table uses 0x01; may be used as a current entry depth value
	//uint8 id;
	uint16 typeId;
	uint16 blank;
	uint32 labelOffset;
	uint32 eHeadSize; // = sizeof(RCOEntry) = 0x28 [ only used for entries with extra info (ie not "main" entries) ]
	uint32 entrySize; // main tables (main/img etc) uses 0x28 here, or is this the length of current entry (not including subentries)?
	// 0x10
	uint32 numSubentries;
	uint32 nextEntryOffset;
	uint32 prevEntryOffset; // this is usually 0x0 however (does make writing RCOs easier though :P I guess Sony's tools do something similar...)
	uint32 parentTblOffset; // offset of this entry from parent table
	// 0x20
	uint32 blanks[2];
	
});

PACK_STRUCT(RCOVSMXEntry, {
	uint32 offset; // always 0x0, so I assume this is an offset
	uint32 length; // length of VSMX file
});

#define RCO_LANG_JAPANESE  0x0
#define RCO_LANG_ENGLISH  0x1
#define RCO_LANG_FRENCH  0x2
#define RCO_LANG_SPANISH  0x3
#define RCO_LANG_GERMAN  0x4
#define RCO_LANG_ITALIAN  0x5
#define RCO_LANG_DUTCH  0x6
#define RCO_LANG_PORTUGESE  0x7
#define RCO_LANG_RUSSIAN  0x8
#define RCO_LANG_KOREAN  0x9
#define RCO_LANG_CHINESE_TRADITIOAL  0xA
#define RCO_LANG_CHINESE_SIMPLIFIED  0xB
// the following are just guestimated from the p3tcompiler readme
#define RCO_LANG_FINNISH  0xC
#define RCO_LANG_SWEDISH  0xD
#define RCO_LANG_DANISH  0xE
#define RCO_LANG_NORWEGIAN  0xF
#define RCO_TEXT_FMT_UTF8 0x0
#define RCO_TEXT_FMT_UTF16 0x1
#define RCO_TEXT_FMT_UTF32 0x2
PACK_STRUCT(RCOTextEntry, {
	uint16 lang;
	uint16 format;
	uint32 numIndexes;
});

PACK_STRUCT(RCOTextIndex, {
	uint32 labelOffset;
	uint32 length;
	uint32 offset;
});

#define RCO_DATA_COMPRESSION_NONE 0x0
#define RCO_DATA_COMPRESSION_ZLIB 0x1
#define RCO_DATA_COMPRESSION_RLZ 0x2

#define RCO_IMG_PNG  0x0
#define RCO_IMG_JPEG  0x1
#define RCO_IMG_TIFF  0x2
#define RCO_IMG_GIF  0x3
#define RCO_IMG_BMP  0x4
#define RCO_IMG_GIM  0x5
#define RCO_MODEL_GMO 0x0
PACK_STRUCT(RCOImgModelEntry, {
	uint16 format;
	uint16 compression; // RCO_DATA_COMPRESSION_* constants
	uint32 sizePacked;
	uint32 offset;
	uint32 sizeUnpacked; // this value doesn't exist if entry isn't compressed
});
// note, some image/model entries which aren't compressed, don't have the last member
PACK_STRUCT(RCOPS3ImgModelEntry, { // PS3 version of the above
	uint16 format;
	uint16 compression; // RCO_DATA_COMPRESSION_* constants
	uint32 sizePacked;
	uint32 offset;
	uint32 something; // PS3 RCOs seem to have this extra element - probably something to do with planes/frames?? usually 0x1
	uint32 sizeUnpacked; // this value doesn't exist if entry isn't compressed
});


#define RCO_SOUND_VAG 0x1
PACK_STRUCT(RCOSoundEntry, {
	uint16 format;  // 0x01 = VAG
	uint16 channels; // 1 or 2 channels
	uint32 sizeTotal;
	uint32 offset;
	// now pairs of size/offset for each channel
});


PACK_STRUCT(RCOFontEntry, {
	uint16 format; // 1
	uint16 compression; // 0
	uint32 unknown; // 0
	uint32 unknown2;
});

#define RCO_OBJ_TYPE_PAGE		0x1
#define RCO_OBJ_TYPE_PLANE		0x2
#define RCO_OBJ_TYPE_BUTTON		0x3
#define RCO_OBJ_TYPE_XMENU		0x4
#define RCO_OBJ_TYPE_XMLIST		0x5
#define RCO_OBJ_TYPE_XLIST		0x6
#define RCO_OBJ_TYPE_PROGRESS		0x7
#define RCO_OBJ_TYPE_SCROLL		0x8
#define RCO_OBJ_TYPE_MLIST		0x9
#define RCO_OBJ_TYPE_MITEM		0xA
		//#define RCO_OBJ_TYPE_0B	= 0xB
#define RCO_OBJ_TYPE_XITEM		0xC
#define RCO_OBJ_TYPE_TEXT		0xD
#define RCO_OBJ_TYPE_MODEL		0xE
#define RCO_OBJ_TYPE_SPIN		0xF
#define RCO_OBJ_TYPE_ACTION		0x10
#define RCO_OBJ_TYPE_ITEMSPIN	0x11
#define RCO_OBJ_TYPE_GROUP		0x12
#define RCO_OBJ_TYPE_LLIST		0x13
#define RCO_OBJ_TYPE_LITEM		0x14
#define RCO_OBJ_TYPE_EDIT		0x15
#define RCO_OBJ_TYPE_CLOCK		0x16
#define RCO_OBJ_TYPE_ILIST		0x17
#define RCO_OBJ_TYPE_IITEM		0x18
#define RCO_OBJ_TYPE_ICON		0x19
#define RCO_OBJ_TYPE_UBUTTON	0x1A

#define RCO_REF_EVENT	0x400 // relative
#define RCO_REF_TEXT	0x401 // # (0-based)
#define RCO_REF_IMG		0x402 // absolute
#define RCO_REF_MODEL	0x403 // absolute
#define RCO_REF_FONT	0x405 // absolute
#define RCO_REF_OBJ2	0x407 // same as 0x409??
#define RCO_REF_ANIM	0x408 // absolute
#define RCO_REF_OBJ		0x409 // absolute
#define RCO_REF_NONE	0xFFFF
PACK_STRUCT(RCOReference, {
	uint32 type;
	uint32 ptr;
});


#define RCO_ANIM_TYPE_POS		0x2
#define RCO_ANIM_TYPE_COLOUR	0x3
#define RCO_ANIM_TYPE_ROTATE	0x4
#define RCO_ANIM_TYPE_SCALE		0x5
#define RCO_ANIM_TYPE_ALPHA		0x6
#define RCO_ANIM_TYPE_DELAY		0x7
#define RCO_ANIM_TYPE_EVENT		0x8
#define RCO_ANIM_TYPE_LOCK		0x9 // ?
#define RCO_ANIM_TYPE_UNLOCK	0xA // ?
#define RCO_ANIM_TYPE_0B		0xB // only appears on UMD RCOs?




PACK_STRUCT(HeaderComprInfo, {
	uint32 lenPacked;
	uint32 lenUnpacked;
	uint32 lenLongestText; // length of the longest language's text data (unpacked)
});

PACK_STRUCT(TextComprInfo, {
	uint16 lang;
	uint16 unknown; // always 0x1
	uint32 nextOffset; // = ALIGN_TO_4(sizeof(TextComprInfo) + packedLen); is 0 for last text entry regardless of what actually comes after
	uint32 packedLen;
	uint32 unpackedLen;
});



#endif
