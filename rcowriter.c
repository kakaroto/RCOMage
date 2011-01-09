// main todos: src duplication checker in rco_write_resource()

#include <stdlib.h> // malloc & free
#include <zlib.h> // compressBound()
#include <string.h> // memset()
#include "general.h"
#include "rcomain.h"
#include "rcofile.h"


typedef struct {
	FILE* fp; // for direct output
	
	
	rRCOFile* rco;
	
	
	// tracking size of resources
	uint32 sizeImg, sizeModel, sizeSound;
	uint32 sizeText;
	
	uint32 longestLangData; // just for keeping track for text compression
	
	
	// memory compression thing
	void* tables;
	uint tablesSize;
	uint tablesBuffered; // internal value used for buffering, following define is the amount to increase the buffer by when necessary
	#define RCO_WRITE_MEM_BUFFER 65536
	uint memPos;
	uint memOffset; // should always be 0xA4
	
} rRCOFile_writehelper;


uint write_entry(rRCOFile_writehelper* rcoH, rRCOEntry* entry, uint parentOffset, uint prevOffset, Bool isLastSubentry);
uint rcowrite_ftell(rRCOFile_writehelper* rcoH);
void rco_fwrite(rRCOFile_writehelper* rcoH, void* buffer, uint len);
void rcowrite_fseek(rRCOFile_writehelper* rcoH, uint pos);

uint rco_write_resource(FILE* dest, rRCOEntry* entry, uint destCompression, writerco_options* opts, rRCOFile* rco);
uint rco_write_text_resource(rRCOFile_writehelper* rcoH, rRCOEntry* entry, uint destCompression, writerco_options* opts, uint lang, Bool isLast);


void rco_write_fix_refs(rRCOEntry* parent, rRCOFile_writehelper* rcoH, rRCOFile* rco, const int* lenArray, const int lenNum, Bool isObj);

uint write_hash_table(rRCOFile_writehelper* rcoH, rRCOEntry* entry, rRCOFile* rco);
uint write_text_hash_table(rRCOFile_writehelper* rcoH, rRCOEntry* entry, rRCOFile* rco);
void do_hashing(rRCOEntry* entry, rRCOFile* rco, Bool recurse, uint32* hashTable, uint hashTableSize);
uint calc_hash(char* in, uint32* hashTable, uint hashTableSize);
int text_hash_table_qsort(const rRCOEntry** a, const rRCOEntry** b);

// packing: use RCO_DATA_COMPRESSION_* constants
Bool write_rco(rRCOFile* rco, char* fn, writerco_options opts) {
	uint i;
	rRCOFile_writehelper rcoH;
	
	// delete file if exists
	if(file_exists(fn)) {
		if(remove(fn)) {
			error("Unable to write to file %s", fn);
			return FALSE;
		}
	}
	
	
	rcoH.rco = rco;
	rcoH.fp = fopen(fn, "wb");
	if(!rcoH.fp) {
		error("Unable to open file %s", fn);
		return FALSE;
	}
	
	
	PRFHeader header;
	header.signature = RCO_SIGNATURE;
	header.version = (opts.packHeader == RCO_DATA_COMPRESSION_RLZ ? 0x95 : opts.packHeader == RCO_DATA_COMPRESSION_ZLIB ? 0x90 : 0x71);
	if(rco->verId) { // we won't actually use specified value, rather, we'll require using the minimum version from above
		if(rco->verId > header.version)
			header.version = rco->verId;
	}
	header.null = 0;
	header.compression = (opts.packHeader << 4) | (rco->umdFlag & 0xF);
	
	header.pMainTable = 0xA4; // pretty much always the case
	// set other sections to nothing for now
	header.pVSMXTable = header.pTextTable = header.pSoundTable = header.pModelTable = header.pImgTable = header.pObjTable = header.pAnimTable = RCO_NULL_PTR;
	header.pUnknown = header.pFontTable = RCO_NULL_PTR;
	
	// don't know positions of text/label/event data too, but we do know the lengths for label/events
	//header.pTextData = header.pLabelData = header.pEventData = 0;
	header.lLabelData = rco->labelsLen;
	header.lEventData = rco->eventsLen;
	header.lTextData = 0;
	
	// set pointer sections to blank too
	header.pTextPtrs = header.pImgPtrs = header.pModelPtrs = header.pSoundPtrs = header.pObjPtrs = header.pAnimPtrs = RCO_NULL_PTR;
	
	header.lTextPtrs = header.lImgPtrs = header.lModelPtrs = header.lSoundPtrs = header.lObjPtrs = header.lAnimPtrs = 0;
	
	// also blank...
	header.pImgData = header.pSoundData = header.pModelData = RCO_NULL_PTR;
	header.lImgData = header.lSoundData = header.lModelData = 0;
	
	header.unknown[0] = header.unknown[1] = header.unknown[2] = 0xFFFFFFFF;
	
	
	
	
	// write resources to a separate file to get around the issue of unknown packed size when writing the header (and you can't change it backed after the header is packed...)
	FILE* fTmp = NULL;
	if((rco->tblImage && rco->tblImage->numSubentries)
	 ||(rco->tblSound && rco->tblSound->numSubentries)
	 ||(rco->tblModel && rco->tblModel->numSubentries)) {
		uint32 totalPackedLen = 0;
		rRCOEntry* rcoNode;
		fTmp = tmpfile();
		
		if(rco->tblImage && rco->tblImage->numSubentries) {
			for(rcoNode=rco->tblImage->firstChild; rcoNode; rcoNode=rcoNode->next) {
				// our compression decision thing
				uint c = ((rRCOImgModelEntry*)(rcoNode->extra))->compression;
				if(((rRCOImgModelEntry*)(rcoNode->extra))->format < RCO_IMG_BMP) {
					if(opts.packImgCompr != -1)
						c = opts.packImgCompr;
				}
				else {
					if(opts.packImg != -1)
						c = opts.packImg;
				}
				
				if(rcoNode->srcLenUnpacked) {
					rcoNode->srcLen = rco_write_resource(fTmp, rcoNode, c, &opts, rco);
					if(!rcoNode->srcLen && rcoNode->labelOffset != RCO_NULL_PTR) warning("[resource] Can't write image resource '%s'!", rco->labels + rcoNode->labelOffset);
				}
				rcoNode->srcCompression = c;
				rcoNode->srcAddr = totalPackedLen;
				
				totalPackedLen += (rcoNode->srcLen % 4 ? (rcoNode->srcLen/4)*4+4 : rcoNode->srcLen);
			}
			header.lImgData = totalPackedLen;
		}
		
		totalPackedLen = 0;
		if(rco->tblSound && rco->tblSound->numSubentries) {
			for(rcoNode=rco->tblSound->firstChild; rcoNode; rcoNode=rcoNode->next) {
				if(rcoNode->srcLenUnpacked) {
					uint packedLen = rco_write_resource(fTmp, rcoNode, RCO_DATA_COMPRESSION_NONE, &opts, rco);
					if(!packedLen && rcoNode->labelOffset != RCO_NULL_PTR) warning("[resource] Can't write sound resource '%s'!", rco->labels + rcoNode->labelOffset);
					totalPackedLen += ALIGN_TO_4(packedLen);
					//if(totalPackedLen %4) totalPackedLen += 4-(totalPackedLen%4);
				}
			}
			header.lSoundData = totalPackedLen;
		}
		
		totalPackedLen = 0;
		if(rco->tblModel && rco->tblModel->numSubentries) {
			for(rcoNode=rco->tblModel->firstChild; rcoNode; rcoNode=rcoNode->next) {
				uint c = ((rRCOImgModelEntry*)(rcoNode->extra))->compression;
				if(opts.packModel != -1)
					c = opts.packModel;
				
				if(rcoNode->srcLenUnpacked) {
					rcoNode->srcLen = rco_write_resource(fTmp, rcoNode, c, &opts, rco);
					if(!rcoNode->srcLen && rcoNode->labelOffset != RCO_NULL_PTR) warning("[resource] Can't write model resource '%s'!", rco->labels + rcoNode->labelOffset);
				}
				rcoNode->srcCompression = c;
				rcoNode->srcAddr = totalPackedLen;
				
				totalPackedLen += (rcoNode->srcLen % 4 ? (rcoNode->srcLen/4)*4+4 : rcoNode->srcLen);
			}
			header.lModelData = totalPackedLen;
		}
		
		rewind(fTmp);
	}
	
	
	
	filewrite(rcoH.fp, &header, sizeof(header));
	
	rcoH.tables = 0;
	
	// if compressing, write to memory
	if(opts.packHeader) {
		rcoH.tables = malloc(RCO_WRITE_MEM_BUFFER);
		rcoH.memPos = rcoH.tablesSize = 0;
		rcoH.tablesBuffered = RCO_WRITE_MEM_BUFFER;
		rcoH.memOffset = ftell(rcoH.fp);
	}
	
	rcoH.sizeImg = rcoH.sizeModel = rcoH.sizeSound = rcoH.sizeText = 0;
	rcoH.longestLangData = 0;
	
	write_entry(&rcoH, &(rco->tblMain), 0xA4 /* typically where the main table is written to */, 0, TRUE);
	
	// fix up object/anim extra data
	{
		if(rco->tblObj)
			rco_write_fix_refs(rco->tblObj, &rcoH, rco, RCO_OBJ_EXTRA_LEN, RCO_OBJ_EXTRA_LEN_NUM, TRUE);
		if(rco->tblAnim)
			rco_write_fix_refs(rco->tblAnim, &rcoH, rco, RCO_ANIM_EXTRA_LEN, RCO_ANIM_EXTRA_LEN_NUM, FALSE);
	}
	
	{ // write hashtable data
		
		/* { // special case for text hashes
			if(rco->numPtrText) {
				header.pTextPtrs = rcowrite_ftell(&rcoH);
				for(i=0; i<rco->numPtrText; i++) {
					uint32 writePtr = 0;
					if(rco->ptrText[i].textEntry && rco->ptrText[i].index)
						writePtr = rco->ptrText[i].textEntry->offset + sizeof(RCOEntry) + sizeof(RCOTextEntry) + (rco->ptrText[i].index - ((rRCOTextEntry*)(rco->ptrText[i].textEntry->extra))->indexes)*sizeof(RCOTextIndex);
					rco_fwrite(&rcoH, &writePtr, sizeof(uint32));
				}
			}
		} */
		if(rco->tblText) {
			header.pTextPtrs = rcowrite_ftell(&rcoH);
			header.lTextPtrs = 0;
			
			// generate sorted list of text entries, sorted by languageID
			rRCOEntry** sList = make_sorted_list_of_subentries(rco->tblText, text_hash_table_qsort);
			
			for(i=0; i<rco->tblText->numSubentries; i++)
				header.lTextPtrs += write_text_hash_table(&rcoH, sList[i], rco);
			free(sList);
			
			header.lTextPtrs *= sizeof(uint32);
		}
		
		if(rco->tblImage) {
			header.pImgPtrs = rcowrite_ftell(&rcoH);
			header.lImgPtrs = write_hash_table(&rcoH, rco->tblImage, rco) * sizeof(uint32);
		}
		if(rco->tblModel) {
			header.pModelPtrs = rcowrite_ftell(&rcoH);
			header.lModelPtrs = write_hash_table(&rcoH, rco->tblModel, rco) * sizeof(uint32);
		}
		if(rco->tblSound) {
			header.pSoundPtrs = rcowrite_ftell(&rcoH);
			header.lSoundPtrs = write_hash_table(&rcoH, rco->tblSound, rco) * sizeof(uint32);
		}
		if(rco->tblObj) {
			header.pObjPtrs = rcowrite_ftell(&rcoH);
			header.lObjPtrs = write_hash_table(&rcoH, rco->tblObj, rco) * sizeof(uint32);
		}
		if(rco->tblAnim) {
			header.pAnimPtrs = rcowrite_ftell(&rcoH);
			header.lAnimPtrs = write_hash_table(&rcoH, rco->tblAnim, rco) * sizeof(uint32);
		}
		/*
		#define RCO_WRITERCO_WRITE_PTR_SECT(pd, pl, hp) { \
			if(pl) { \
				hp = rcowrite_ftell(&rcoH); \
				for(i=0; i<pl; i++) { \
					if(pd[i]) \
						rco_fwrite(&rcoH, &(((rRCOEntry*)(pd[i]))->offset), sizeof(uint32)); \
					else { \
						uint32 zero = 0; \
						rco_fwrite(&rcoH, &zero, sizeof(uint32)); \
					} \
				} \
			} \
		}
		//RCO_WRITERCO_WRITE_PTR_SECT(rco->ptrText, rco->numPtrText, header.pTextPtrs);
		RCO_WRITERCO_WRITE_PTR_SECT(rco->ptrImg, rco->numPtrImg, header.pImgPtrs);
		RCO_WRITERCO_WRITE_PTR_SECT(rco->ptrModel, rco->numPtrModel, header.pModelPtrs);
		RCO_WRITERCO_WRITE_PTR_SECT(rco->ptrSound, rco->numPtrSound, header.pSoundPtrs);
		RCO_WRITERCO_WRITE_PTR_SECT(rco->ptrObj, rco->numPtrObj, header.pObjPtrs);
		RCO_WRITERCO_WRITE_PTR_SECT(rco->ptrAnim, rco->numPtrAnim, header.pAnimPtrs);
		*/
	}
	
	
	{ // write label/event data (and text if applicable)
		
		// write text (note, old behaviour - newer RCOs have text written in a different location)
		if(!opts.packText && rco->tblText && rco->tblText->numSubentries) {
			rRCOEntry* rcoNode;
			header.pTextData = rcowrite_ftell(&rcoH);
			header.lTextData = rcoH.sizeText;
			for(rcoNode=rco->tblText->firstChild; rcoNode; rcoNode=rcoNode->next) {
				rco_write_text_resource(&rcoH, rcoNode, RCO_DATA_COMPRESSION_NONE, &opts, ((rRCOTextEntry*)(rcoNode->extra))->lang, (rco->tblText->lastChild == rcoNode));
			}
		}
		
		
		// write label+event data
		header.pLabelData = rcowrite_ftell(&rcoH);
		if(rco->labelsLen)
			rco_fwrite(&rcoH, rco->labels, rco->labelsLen);
		header.pEventData = rcowrite_ftell(&rcoH);
		if(rco->eventsLen)
			rco_fwrite(&rcoH, rco->events, rco->eventsLen);
		else if(rco->tblObj || rco->tblAnim) { // weird case: if there's object entries, there will be 4 bytes for events; I'll assume this covers anim as well (although there isn't an RCO with anim that doesn't have objects)
			uint32 zero = 0;
			rco_fwrite(&rcoH, &zero, sizeof(zero));
			header.lEventData = sizeof(zero);
		}
		
		// the text pointer is weird in that if there's no text, it's set equal to the label pointer; even weirder, some RCOs have a null pointer (for FW5.00 all except lftv_* RCOs have null pointers for pTextData if there's no text)
		// my theory: if compressing, it will be RCO_NULL_PTR, otherwise it'll = header.pLabelData
		//if(!header.lTextData) header.pTextData = RCO_NULL_PTR;
		//if(!header.lTextData) header.pTextData = header.pLabelData;
		if(!header.lTextData) header.pTextData = (opts.packHeader ? RCO_NULL_PTR : header.pLabelData);
	}
	
	// flush compression stuff here
	HeaderComprInfo ci;
	if(opts.packHeader) {
		uint8* bufferOut = NULL;
		ci.lenLongestText = rcoH.longestLangData;
		ci.lenUnpacked = rcoH.tablesSize;
		ci.lenPacked = 0;
		
		if(opts.packHeader == RCO_DATA_COMPRESSION_ZLIB) {
			uint bound = compressBound(rcoH.tablesSize);
			bufferOut = (uint8*)malloc(bound);
			ci.lenPacked = zlib_compress(rcoH.tables, rcoH.tablesSize, bufferOut, bound, opts.zlibLevel, opts.zlibMethod);
		} else if(opts.packHeader == RCO_DATA_COMPRESSION_RLZ) {
			bufferOut = (uint8*)malloc(rcoH.tablesSize);
			ci.lenPacked = rlz_compress(rcoH.tables, rcoH.tablesSize, bufferOut, rcoH.tablesSize, opts.rlzMode);
		} else {
			error("lulwut?");
			exit(1);
		}
		int comprMisalign = ci.lenPacked % 4;
		uint packedLen = ci.lenPacked;
		
		if(rco->eSwap) es_headerComprInfo(&ci);
		filewrite(rcoH.fp, &ci, sizeof(ci));
		filewrite(rcoH.fp, bufferOut, packedLen);
		free(bufferOut);
		
		if(comprMisalign) { // 4 byte align
			uint32 zero = 0;
			filewrite(rcoH.fp, &zero, 4 - comprMisalign);
		}
	}
	
	// write text if packing header
	if(opts.packText && rco->tblText && rco->tblText->numSubentries) {
		rRCOEntry* rcoNode;
		//header.pTextData = rcowrite_ftell(&rcoH);
		header.pTextData = ftell(rcoH.fp);
		header.lTextData = 0; // rcoH.sizeText;
		for(rcoNode=rco->tblText->firstChild; rcoNode; rcoNode=rcoNode->next) {
			header.lTextData += rco_write_text_resource(&rcoH, rcoNode, opts.packHeader, &opts, ((rRCOTextEntry*)(rcoNode->extra))->lang, (rco->tblText->lastChild == rcoNode));
		}
	}
	
	// write resources
	/* {
		uint32 totalPackedLen = 0;
		if(rco->tblImage) {
			header.pImgData = rcowrite_ftell(&rcoH);
			header.lImgData = rcoH.sizeImg; // TOxDO: this model actually won't work - we have to update the offsets of ALL the entries after packing...
			for(i=0; i<rco->tblImage->numSubentries; i++) {
				uint32 packedSize = rco_write_resource(&rcoH, &(rco->tblImage->subentries[i]), RCO_DATA_COMPRESSION_NONE); // TOxDO: change this
				// TOxDO: update packed size value
				uint curFpos = rcowrite_ftell(rcoH.fp);
				
				
				totalPackedLen += (packedSize % 4 ? (packedSize/4)*4+4 : packedSize);
			}
			header.lImgData = totalPackedLen;
		}
		
		totalPackedLen = 0;
		if(rco->tblSound) {
			header.pSoundData = rcowrite_ftell(&rcoH);
			header.lSoundData = rcoH.sizeSound;
			for(i=0; i<rco->tblSound->numSubentries; i++) {
				totalPackedLen += rco_write_resource(&rcoH, &(rco->tblSound->subentries[i]), RCO_DATA_COMPRESSION_NONE);
				if(totalPackedLen %4) totalPackedLen += 4-(totalPackedLen%4);
			}
			header.lSoundData = totalPackedLen;
		}
		
		// TOxDO: write model resources
	} */
	
	if((rco->tblImage && rco->tblImage->numSubentries)
	 ||(rco->tblSound && rco->tblSound->numSubentries)
	 ||(rco->tblModel && rco->tblModel->numSubentries)) {
		// update data pointers
		uint32 pos = ftell(rcoH.fp);
		if(rco->tblImage && rco->tblImage->numSubentries) {
			header.pImgData = pos;
			pos += header.lImgData;
		}
		if(rco->tblSound && rco->tblSound->numSubentries) {
			header.pSoundData = pos;
			pos += header.lSoundData;
		}
		if(rco->tblModel && rco->tblModel->numSubentries) {
			header.pModelData = pos;
			pos += header.lModelData;
		}
		// copy contents of fTmp across (uses a simple buffered copy)
		uint len = header.lImgData + header.lSoundData + header.lModelData;
		uint8 buffer[65536];
		while(len) {
			uint readAmt = (len > 65536 ? 65536 : len);
			fileread(fTmp, buffer, readAmt);
			filewrite(rcoH.fp, buffer, readAmt);
			len -= readAmt;
		}
		
		fclose(fTmp); // this deletes our temp file
	}
	
	
	// fix header
	if(rco->tblVSMX) header.pVSMXTable = rco->tblVSMX->offset;
	if(rco->tblText) header.pTextTable = rco->tblText->offset;
	if(rco->tblSound) header.pSoundTable = rco->tblSound->offset;
	if(rco->tblModel) header.pModelTable = rco->tblModel->offset;
	if(rco->tblImage) header.pImgTable = rco->tblImage->offset;
	if(rco->tblFont) header.pFontTable = rco->tblFont->offset;
	if(rco->tblObj) header.pObjTable = rco->tblObj->offset;
	if(rco->tblAnim) header.pAnimTable = rco->tblAnim->offset;
	
	rewind(rcoH.fp);
	if(rco->eSwap) es_rcoHeader(&header);
	filewrite(rcoH.fp, &header, sizeof(header));
	
	// TODO: fix resource pointers?
	// TODO: tie things up etc??
	
	
	fclose(rcoH.fp);
	
	return TRUE;
}

// returns next entry offset (like the length, but the last entry returns zero) - doesn't really have much meaning - it's primarily used for internal purposes
uint write_entry(rRCOFile_writehelper* rcoH, rRCOEntry* entry, uint parentOffset, uint prevOffset, Bool isLastSubentry) {
	uint fPos = rcowrite_ftell(rcoH);
	entry->offset = fPos;
	RCOEntry re;
	re.typeId = (entry->id << 8) | (entry->type);
	re.blank = 0;
	re.labelOffset = entry->labelOffset;
	
	if(entry->type == 0 || ((entry->id == RCO_TABLE_MAIN || entry->id == RCO_TABLE_ANIM) && entry->type == 1)) {
		// a "parent" entry
		re.eHeadSize = 0;
		re.entrySize = sizeof(RCOEntry);
		
		// anim main tables have a prevOffset though :/ (object main tables don't)
		if(entry->id == RCO_TABLE_ANIM)
			re.prevEntryOffset = prevOffset;
		else
			re.prevEntryOffset = 0;
	} else {
		re.eHeadSize = sizeof(RCOEntry);
		re.entrySize = 0;
		
		// not sure why, but it appears that "non-main" object/anim entries also have the total size of the entry stored in entrySize; only done if the entry has subentries
		if((entry->id == RCO_TABLE_OBJ || entry->id == RCO_TABLE_ANIM) && entry->numSubentries) {
			int lenNum = (entry->id == RCO_TABLE_OBJ ? RCO_OBJ_EXTRA_LEN_NUM : RCO_ANIM_EXTRA_LEN_NUM);
			const int* lenArray;
			lenArray = (entry->id == RCO_TABLE_OBJ ? RCO_OBJ_EXTRA_LEN : RCO_ANIM_EXTRA_LEN);
			if(entry->type <= lenNum && lenArray[entry->type] > 0)
				re.entrySize = sizeof(RCOEntry) + lenArray[entry->type] *sizeof(uint32);
		}
		
		re.prevEntryOffset = prevOffset;
	}
	re.numSubentries = entry->numSubentries;
	re.parentTblOffset = fPos - parentOffset;
	re.blanks[0] = re.blanks[1] = 0;
	re.nextEntryOffset = 0;
	
	if(entry->rco->eSwap) es_rcoEntry(&re);
	rco_fwrite(rcoH, &re, sizeof(re));
	if(entry->rco->eSwap) es_rcoEntry(&re);
	
	// extra items here
	switch(entry->id) {
		case RCO_TABLE_VSMX:
			if(entry->type == 1) {
				RCOVSMXEntry rve;
				rve.offset = 0;
				rve.length = entry->srcLenUnpacked;
				if(entry->rco->eSwap) es_rcoVsmxEntry(&rve);
				rco_fwrite(rcoH, &rve, sizeof(rve));
				
				uint vsmxLen=0;
				uint8* bufferVSMX = (uint8*)read_resource(entry, &vsmxLen);
				if(bufferVSMX) {
					if(vsmxLen == entry->srcLenUnpacked)
						rco_fwrite(rcoH, bufferVSMX, vsmxLen);
					free(bufferVSMX);
				}
			}
			break;
		case RCO_TABLE_TEXT:
			if(entry->type == 1) {
				RCOTextEntry rte;
				rRCOTextEntry* src = (rRCOTextEntry*)entry->extra;
				uint i;
				rte.lang = src->lang;
				rte.format = src->format;
				rte.numIndexes = src->numIndexes;
				if(entry->rco->eSwap) es_rcoTextEntry(&rte);
				rco_fwrite(rcoH, &rte, sizeof(rte));
				
				// instead of blindly dumping src->indexes, we'll "pack" the entries together (allows source file to be of a different format, ie INI)
				uint32 entryTextOffset = rcoH->sizeText;
				for(i=0; i<src->numIndexes; i++) {
					RCOTextIndex rti;
					rti.labelOffset = src->indexes[i].labelOffset;
					rti.length = src->indexes[i].length;
					// Sony is doing some weird stuff :|
					if(src->indexes[i].offset == RCO_NULL_PTR)
						rti.offset = RCO_NULL_PTR;
					else if(rti.length) {
						if(rcoH->tables) // compressing - we need to make the offset relative to the section of text data
							rti.offset = rcoH->sizeText - entryTextOffset;
						else
							rti.offset = rcoH->sizeText;
					}
					else // TODO: experimental (it seems that Sony likes sticking in a weird pointer for a blank text entry)
						rti.offset = entryTextOffset -1;
					
					rcoH->sizeText += rti.length; // doesn't have trailing null, so no +1 needed
					// align to 4 byte boundary
					rcoH->sizeText = ALIGN_TO_4(rcoH->sizeText);
					
					if(entry->rco->eSwap) es_rcoTextIndex(&rti);
					rco_fwrite(rcoH, &rti, sizeof(rti));
				}
				
				if(rcoH->sizeText - entryTextOffset > rcoH->longestLangData)
					rcoH->longestLangData = rcoH->sizeText - entryTextOffset;
			}
			break;
		case RCO_TABLE_IMG: case RCO_TABLE_MODEL:
			if(entry->type == 1) {
				rRCOImgModelEntry* srcExtra = (rRCOImgModelEntry*)entry->extra;
				RCOImgModelEntry rie;
				uint32* totalResSize = (entry->id == RCO_TABLE_IMG ? &(rcoH->sizeImg) : &(rcoH->sizeModel));
				rie.format = srcExtra->format;
				// we've already got the compression info done here, so just copy it over
				rie.compression = entry->srcCompression | (srcExtra->unkCompr << 8);
				rie.offset = *totalResSize;
				rie.sizeUnpacked = entry->srcLenUnpacked;
				rie.sizePacked = entry->srcLen;
				*totalResSize += rie.sizePacked;
				// align to 4 byte boundary
				*totalResSize = ALIGN_TO_4(*totalResSize);
				
				if(entry->rco->eSwap) es_rcoImgModelEntry(&rie);
				
				// we'll omit the packed length value if this is an uncompressed entry
				if(entry->rco->ps3) { // PS3 image quirk
					uint32 one = ENDIAN_SWAP(1);
					rco_fwrite(rcoH, &rie, sizeof(rie) - sizeof(uint32));
					rco_fwrite(rcoH, &one, sizeof(one));
					if(entry->srcCompression != RCO_DATA_COMPRESSION_NONE)
						rco_fwrite(rcoH, &rie.sizeUnpacked, sizeof(rie.sizeUnpacked));
				} else {
					rco_fwrite(rcoH, &rie, sizeof(rie) - (entry->srcCompression == RCO_DATA_COMPRESSION_NONE ? sizeof(uint32) : 0));
				}
			}
			break;
		case RCO_TABLE_SOUND:
			if(entry->type != 0) {
				rRCOSoundEntry* srcExtra = (rRCOSoundEntry*)entry->extra;
				RCOSoundEntry rse;
				uint rseOffset;
				rse.format = srcExtra->format;
				rseOffset = rse.offset = rcoH->sizeSound;
				rse.channels = srcExtra->channels;
				rse.sizeTotal = entry->srcLenUnpacked;
				
				rcoH->sizeSound += rse.sizeTotal;
				// align to 4 byte boundary
				rcoH->sizeSound = ALIGN_TO_4(rcoH->sizeSound);
				
				if(entry->rco->eSwap) es_rcoSoundEntry(&rse);
				rco_fwrite(rcoH, &rse, sizeof(rse));
				
				// write size/offset pairs
				uint i;
				// TODO: might actually restrict this to two channels later on
				for(i=0; i<srcExtra->channels; i++) {
					uint32 stuffToWrite[] = {srcExtra->channelData[i*2], srcExtra->channelData[i*2+1] + rseOffset};
					if(entry->rco->eSwap) {
						stuffToWrite[0] = ENDIAN_SWAP(stuffToWrite[0]);
						stuffToWrite[1] = ENDIAN_SWAP(stuffToWrite[1]);
					}
					rco_fwrite(rcoH, stuffToWrite, sizeof(stuffToWrite));
				}
				if(srcExtra->channels < 2) {
					// we'll write an extra blank channel, complying with how Sony's RCO tools work
					uint32 stuffToWrite[] = {0, RCO_NULL_PTR};
					uint i;
					// actually, the following is unnecessary, but we'll keep it here for reference sake
					if(entry->rco->eSwap) {
						stuffToWrite[0] = ENDIAN_SWAP(stuffToWrite[0]);
						stuffToWrite[1] = ENDIAN_SWAP(stuffToWrite[1]);
					}
					for(i=srcExtra->channels; i<2; i++)
						rco_fwrite(rcoH, &stuffToWrite, sizeof(uint32)*2);
				}
			}
			break;
		case RCO_TABLE_FONT:
			if(entry->type == 1) {
				RCOFontEntry rfe;
				rRCOFontEntry* srcExtra = (rRCOFontEntry*)entry->extra;
				rfe.format = srcExtra->format;
				rfe.compression = srcExtra->compression;
				rfe.unknown = srcExtra->unknown;
				rfe.unknown2 = srcExtra->unknown2;
				if(entry->rco->eSwap) es_rcoFontEntry(&rfe);
				rco_fwrite(rcoH, &rfe, sizeof(rfe));
			}
			break;
		case RCO_TABLE_OBJ: case RCO_TABLE_ANIM:
			if(entry->type != 0) {
				int lenNum;
				lenNum = (entry->id == RCO_TABLE_OBJ ? RCO_OBJ_EXTRA_LEN_NUM : RCO_ANIM_EXTRA_LEN_NUM);
				const int* lenArray;
				lenArray = (entry->id == RCO_TABLE_OBJ ? RCO_OBJ_EXTRA_LEN : RCO_ANIM_EXTRA_LEN);
				// just allocate space because we need to fix this later on
				if(entry->type <= lenNum && lenArray[entry->type] > 0) {
					uint32 anAwesomeVariable = lenArray[entry->type];
					while(anAwesomeVariable--)
						rco_fwrite(rcoH, &anAwesomeVariable, sizeof(anAwesomeVariable));
				}
				else {
					// TODO: do something if type is unknown
				}
			}
			break;
	}
	
	// subentries
	uint nextOffset = 0;
	rRCOEntry* rcoNode;
	for(rcoNode=entry->firstChild; rcoNode; rcoNode=rcoNode->next) {
		nextOffset = write_entry(rcoH, rcoNode, fPos, nextOffset, (rcoNode->next ? FALSE:TRUE));
	}
	
	
	// write nextEntryOffset
	if(!isLastSubentry) {
		uint curPos = rcowrite_ftell(rcoH);
		re.nextEntryOffset = curPos - fPos;
		rcowrite_fseek(rcoH, fPos);
		if(entry->rco->eSwap) es_rcoEntry(&re);
		rco_fwrite(rcoH, &re, sizeof(re));
		if(entry->rco->eSwap) es_rcoEntry(&re);
		rcowrite_fseek(rcoH, curPos);
	}
	return re.nextEntryOffset;
}


void rco_fwrite(rRCOFile_writehelper* rcoH, void* buffer, uint len) {
	// memory writing
	if(rcoH->tables) {
		uint len4 = ALIGN_TO_4(len);
		if(rcoH->memPos + len4 > rcoH->tablesBuffered) {
			rcoH->tablesBuffered = rcoH->memPos + len4 + RCO_WRITE_MEM_BUFFER;
			rcoH->tables = realloc(rcoH->tables, rcoH->tablesBuffered);
		}
		
		memcpy((uint8*)rcoH->tables + rcoH->memPos, buffer, len);
		if(len % 4) {
			memset((uint8*)rcoH->tables + rcoH->memPos + len, 0, 4-(len%4));
		}
		rcoH->memPos += len4;
		if(rcoH->memPos > rcoH->tablesSize)
			rcoH->tablesSize = rcoH->memPos;
		
	} else {
		filewrite(rcoH->fp, buffer, len);
		
		// always add 4 byte padding (should add an argument, but will always be TRUE anyway, so I cbf)
		if(len % 4) {
			uint32 zero=0;
			filewrite(rcoH->fp, &zero, 4-(len%4));
		}
	}
}
uint rcowrite_ftell(rRCOFile_writehelper* rcoH) {
	// memory stuff
	if(rcoH->tables) return rcoH->memPos + rcoH->memOffset;
	else return ftell(rcoH->fp);
}
void rcowrite_fseek(rRCOFile_writehelper* rcoH, uint pos) {
	// memory stuff
	if(rcoH->tables) rcoH->memPos = pos - rcoH->memOffset;
	else fseek(rcoH->fp, pos, SEEK_SET);
}

// returns the length of the packed data
// TBH, I really can't be stuffed writing a buffered copy/compress/decompressor (and anyway, it may not work with future compression routines, so meh)
uint rco_write_resource(FILE* dest, rRCOEntry* entry, uint destCompression, writerco_options* opts, rRCOFile* rco) {
	
	uint len=0;
	uint8* bufferMid = (uint8*)read_resource(entry, &len);
	if(!bufferMid) {
		if(entry->labelOffset)
			warning("Failed to read resource '%s'.", rco->labels + entry->labelOffset);
		return 0;
	}
	
	if(len != entry->srcLenUnpacked) {
		free(bufferMid);
		return 0;
	}
	
	uint8* bufferOut;
	uint packedSize = 0;
	if(destCompression == RCO_DATA_COMPRESSION_ZLIB) {
		uint compSize = compressBound(entry->srcLenUnpacked);
		bufferOut = (uint8*)malloc(compSize);
		packedSize = zlib_compress(bufferMid, entry->srcLenUnpacked, bufferOut, compSize, opts->zlibLevel, opts->zlibMethod);
		if(!packedSize) {
			if(entry->labelOffset)
				warning("Failed to compress resource '%s'.", rco->labels + entry->labelOffset);
			return 0;
		}
		
		free(bufferMid);
	} else if(destCompression == RCO_DATA_COMPRESSION_RLZ) {
		#ifdef DISABLE_RLZ
		error("RLZ compression not supported.");
		exit(1);
		#endif
		bufferOut = (uint8*)malloc(entry->srcLenUnpacked);
		packedSize = rlz_compress(bufferMid, entry->srcLenUnpacked, bufferOut, entry->srcLenUnpacked, opts->rlzMode);
		if(!packedSize) {
			if(entry->labelOffset)
				warning("Failed to compress resource '%s'.", rco->labels + entry->labelOffset);
			return 0;
		}
		
		free(bufferMid);
	} else {
		bufferOut = bufferMid;
		packedSize = entry->srcLenUnpacked;
	}
	
	filewrite(dest, bufferOut, packedSize);
	free(bufferOut);
	
	/*
	char buffer[65536], outBuffer[65536];
	uint len=entry->srcLen;
	z_stream out;
	if(destStream == RCO_DATA_COMPRESSION_ZLIB) {
		ZLIB_INIT_DEFLATE(out, 9, Z_DEFAULT_STRATEGY);
		out.next_in = buffer;
		out.avail_in = 65536;
		out.next_out = outBuffer;
		out.avail_out = 65536;
	}
	
	// copy with buffer
	while(len) {
		uint readAmt = (len < 65536 ? len : 65536);
		if(!fileread(src, &buffer, readAmt)) {
			fclose(src);
			return FALSE;
		}
		
		if(destCompression == RCO_DATA_COMPRESSION_NONE)
			filewrite(dest, &buffer, readAmt);
		else if(destCompression == RCO_DATA_COMPRESSION_ZLIB) {
			
		}
		
		len -= readAmt;
	}
	
	// TOxDO: also don't forget to check lenUnpacked
	
	
	fclose(src);
	
	
	if(destStream == RCO_DATA_COMPRESSION_ZLIB)
		deflateEnd(&out);
	*/
	
	// 4byte alignment
	if(packedSize % 4) {
		uint32 zero = 0;
		filewrite(dest, &zero, 4 - (packedSize % 4));
	}
	
	return packedSize;
}

uint rco_write_text_resource(rRCOFile_writehelper* rcoH, rRCOEntry* entry, uint destCompression, writerco_options* opts, uint lang, Bool isLast) {

	uint len=0;
	uint8* bufferMid = (uint8*)read_resource(entry, &len);
	if(!bufferMid) return 0;
	
	if(len != entry->srcLenUnpacked) {
		free(bufferMid);
		return 0;
	}
	
	rRCOTextEntry* extra = (rRCOTextEntry*)(entry->extra);
	
	
	TextComprInfo tci;
	char* textBuffer = NULL;
	uint textBufferPos = 0;
	if(destCompression) {
		tci.lang = lang;
		tci.unknown = 1;
		tci.unpackedLen = 0;
		//textBuffer = (char*)malloc(1);
	}
	
	uint i;
	for(i=0; i<extra->numIndexes; i++) {
		uint len = extra->indexes[i].length;
		if(!len) continue;
		
		if(destCompression) {
			tci.unpackedLen += ALIGN_TO_4(len);
			textBuffer = (char*)realloc(textBuffer, tci.unpackedLen);
			memcpy(textBuffer + textBufferPos, bufferMid + extra->indexes[i].offset, len);
			
			if(len % 4) {
				memset(textBuffer + textBufferPos + len, 0, 4-(len%4));
			}
			textBufferPos += ALIGN_TO_4(len);
		}
		else {
			rco_fwrite(rcoH, bufferMid + extra->indexes[i].offset, len);
		}
	}
	
	free(bufferMid);
	
	if(destCompression) {
		Bool success = TRUE;
		uint8* bufferOut = NULL;
		
		if(destCompression == RCO_DATA_COMPRESSION_ZLIB) {
			uint compSize = compressBound(tci.unpackedLen);
			bufferOut = (uint8*)malloc(compSize);
			tci.packedLen = zlib_compress(textBuffer, tci.unpackedLen, bufferOut, compSize, opts->zlibLevel, opts->zlibMethod);
		} else if(destCompression == RCO_DATA_COMPRESSION_RLZ) {
			#ifdef DISABLE_RLZ
			error("RLZ compression not supported.");
			exit(1);
			#endif
			bufferOut = (uint8*)malloc(tci.unpackedLen);
			tci.packedLen = rlz_compress(textBuffer, tci.unpackedLen, bufferOut, tci.unpackedLen, opts->rlzMode);
		}
		
		if(!tci.packedLen) { // compression failed
			free(bufferOut);
			bufferOut = NULL;
			success = FALSE;
		}
		else if(bufferOut) {
			if(isLast)
				tci.nextOffset = 0;
			else {
				tci.nextOffset = sizeof(tci) + tci.packedLen;
				tci.nextOffset = ALIGN_TO_4(tci.nextOffset);
			}
			if(entry->rco->eSwap) es_textComprInfo(&tci);
			filewrite(rcoH->fp, &tci, sizeof(tci));
			if(entry->rco->eSwap) es_textComprInfo(&tci);
			
			filewrite(rcoH->fp, bufferOut, tci.packedLen);
			free(bufferOut);
			
			if(tci.packedLen % 4) {
				uint32 zero = 0;
				filewrite(rcoH->fp, &zero, 4 - (tci.packedLen % 4));
			}
		} else
			success = FALSE;
		
		free(textBuffer);
		if(!success) return 0;
		return ALIGN_TO_4(tci.packedLen) + sizeof(tci);
	}
	
	return 1;
}

void rco_write_fix_refs(rRCOEntry* parent, rRCOFile_writehelper* rcoH, rRCOFile* rco, const int* lenArray, const int lenNum, Bool isObj) {
	rRCOEntry* rcoNode;
	for(rcoNode=parent->firstChild; rcoNode; rcoNode=rcoNode->next) {
		rcowrite_fseek(rcoH, rcoNode->offset + sizeof(RCOEntry));
		uint j, j2;
		uint8* extraPtr = (uint8*)rcoNode->extra;
		if(rcoNode->type <= lenNum && lenArray[rcoNode->type] > 0) {
			const int* typeArray = (isObj ? RCO_OBJ_EXTRA_TYPES[rcoNode->type] : RCO_ANIM_EXTRA_TYPES[rcoNode->type]);
			for(j=0, j2=0; (int)j<lenArray[rcoNode->type]; j++, j2++) {
				Bool cond;
				if(isObj)
					cond = (RCO_OBJ_IS_REF(rcoNode->type, j2));
				else // anim
					cond = (RCO_ANIM_IS_REF(rcoNode->type, j2));
					//cond = (RCO_ANIM_EXTRA_REFS[rcoNode->type] && j == 0);
				if(cond) {
					rRCORef* srcRef = (rRCORef*)extraPtr;
					RCOReference destRef;
					destRef.type = srcRef->type;
					
					switch(destRef.type) {
						case RCO_REF_EVENT:
							//destRef.ptr = ((char*)(srcRef->ptr)) - rco->events;
							destRef.ptr = srcRef->rawPtr;
							break;
						case RCO_REF_NONE:
							destRef.ptr = RCO_NULL_PTR;
							break;
						case RCO_REF_TEXT:
							destRef.ptr = srcRef->rawPtr;
							break;
						case RCO_REF_IMG: case RCO_REF_MODEL: case RCO_REF_FONT: case RCO_REF_OBJ2: case RCO_REF_ANIM: case RCO_REF_OBJ:
							if(srcRef->ptr)
								destRef.ptr = ((rRCOEntry*)(srcRef->ptr))->offset;
							break;
						default:
							destRef.ptr = srcRef->rawPtr;
					}
					
					if(rco->eSwap) {
						#define ENDIAN_SWAP_HALF32(x) (((x) & 0xFF) << 8 | ((x) & 0xFF00) >> 8 | ((x) & 0xFF0000) << 8 | ((x) & 0xFF000000) >> 8)
						destRef.type = ENDIAN_SWAP_HALF32(destRef.type);
						destRef.ptr = ENDIAN_SWAP(destRef.ptr);
					}
					rco_fwrite(rcoH, &destRef, sizeof(destRef));
					
					extraPtr += sizeof(rRCORef);
					j++;
					//if(!isObj) j2--;
				} else {
					if(rco->eSwap && typeArray[j2] != RCO_OBJ_EXTRA_TYPE_UNK) {
						uint32 val = *(uint32*)extraPtr;
						val = ENDIAN_SWAP(val);
						rco_fwrite(rcoH, &val, sizeof(val));
					} else
						rco_fwrite(rcoH, extraPtr, sizeof(uint32));
					extraPtr += sizeof(uint32);
				}
			}
		}
		else {
			// TODO: think up something for unknown object types
		}
		rco_write_fix_refs(rcoNode, rcoH, rco, lenArray, lenNum, isObj);
	}
}

// returns size of hash table
uint write_hash_table(rRCOFile_writehelper* rcoH, rRCOEntry* entry, rRCOFile* rco) {
	uint num = entry->numSubentries;
	if(entry->id == RCO_TABLE_OBJ)
		num = count_all_subentries(entry);
	//if(!rco->ps3)
	// interestingly, it seems that some PS3 RCOs do not round to an upper prime, but most do
	num = find_larger_prime(num);
	
	if(!num) return 0;
	
	uint32* hashTable = (uint32*)malloc(num * sizeof(uint32));
	memset(hashTable, 0, num * sizeof(uint32));
	rRCOEntry* rcoNode;
	for(rcoNode=entry->firstChild; rcoNode; rcoNode=rcoNode->next)
		do_hashing(rcoNode, rco, (entry->id == RCO_TABLE_OBJ), hashTable, num);
	
	// write it
	rco_fwrite(rcoH, hashTable, num*sizeof(uint32));
	
	free(hashTable);
	return num;
}

uint write_text_hash_table(rRCOFile_writehelper* rcoH, rRCOEntry* entry, rRCOFile* rco) {
	uint num = ((rRCOTextEntry*)entry->extra)->numIndexes;
	if(!num) return 0;
	
	uint32* hashTable = (uint32*)malloc(num * sizeof(uint32));
	memset(hashTable, 0, num * sizeof(uint32));
	uint i;
	for(i=0; i<num; i++) {
		RCOTextIndex* ti = &(((rRCOTextEntry*)entry->extra)->indexes[i]);
		if(ti->labelOffset != RCO_NULL_PTR) {
			uint32* hashPtr = &hashTable[calc_hash(rco->labels + ti->labelOffset, hashTable, num)];
			*hashPtr = entry->offset + sizeof(RCOEntry) + sizeof(RCOTextEntry) + i*sizeof(RCOTextIndex);
			if(rco->eSwap) *hashPtr = ENDIAN_SWAP(*hashPtr);
		}
	}
	
	// write it
	rco_fwrite(rcoH, hashTable, num*sizeof(uint32));
	
	free(hashTable);
	return num;
}

void do_hashing(rRCOEntry* entry, rRCOFile* rco, Bool recurse, uint32* hashTable, uint hashTableSize) {
	if(entry->labelOffset != RCO_NULL_PTR) {
		uint32* hashPtr = &hashTable[calc_hash(rco->labels + entry->labelOffset, hashTable, hashTableSize)];
		*hashPtr = entry->offset;
		if(rco->eSwap) *hashPtr = ENDIAN_SWAP(*hashPtr);
	}
	if(recurse) {
		rRCOEntry* rcoNode;
		for(rcoNode=entry->firstChild; rcoNode; rcoNode=rcoNode->next)
			do_hashing(rcoNode, rco, recurse, hashTable, hashTableSize);
	}
}

uint calc_hash(char* in, uint32* hashTable, uint hashTableSize) {
	uint32 hash = 0;
	// calculate hash code (just a summation of the chars)
	while(*in) {
		hash += *in;
		in++;
	}
	// hash compression
	hash %= hashTableSize;
	
	// linear probing
	while(hashTable[hash]) {
		hash++;
		if(hash >= hashTableSize) hash = 0;
	}
	
	return hash;
}

int text_hash_table_qsort(const rRCOEntry** a, const rRCOEntry** b) {
	return ((rRCOTextEntry*)((*a)->extra))->lang - ((rRCOTextEntry*)((*b)->extra))->lang;
}
