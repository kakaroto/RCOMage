
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iconv.h>

// chdir, mkdir etc
#ifdef WIN32 /* windows */
#include <windows.h>
#include <direct.h>
#define makedir _mkdir
#else /* linux */
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#define makedir(s) mkdir(s, 0777);
#endif


#include "general.h"
#include "rcomain.h"
#include "xml.h"
#include "strfuncs.h"

#include "rcodump.h"
#include "vaghandler.h"
#include "vsmx.h"

Bool exec_gimconv(char* cmd, char* src, char* dest, char* extFlags);


Bool dump_resource(char* dest, rRCOEntry* entry, OutputDumpFunc outputfunc, void* outputfuncArg) {
	
	if(file_exists(dest)) {
		warning("File '%s' already exists.", dest);
		return FALSE;
	}
	
	uint len=0;
	uint8* bufferMid = (uint8*)read_resource(entry, &len);
	if(!bufferMid) return FALSE;
	
	if(!len) {
		free(bufferMid);
		return FALSE;
	}
	
	if(len != entry->srcLenUnpacked)
		warning("Extracted resource size for does not match specified length.");
	
	Bool ret = outputfunc(dest, (void*)bufferMid, entry, outputfuncArg);
	free(bufferMid);
	return ret;
}

Bool dump_output_data(char* dest, void* buf, rRCOEntry* entry, void* arg) {
	FILE* fp = openwrite(dest);
	if(fp) {
		filewrite(fp, buf, entry->srcLenUnpacked);
		fclose(fp);
		return TRUE;
	}
	warning("Unable to open to file '%s'.", dest);
	return FALSE;
}

Bool dump_output_wav(char* dest, void* buf, rRCOEntry* entry, void* arg) {
	int i;
	rRCOSoundEntry* rse = ((rRCOSoundEntry*)entry->extra);
	
	void** vagData = (void**)malloc(rse->channels * sizeof(void*));
	int* vagLen = (int*)malloc(rse->channels * sizeof(int));
	
	Bool ret;
	
	for(i=0; i<rse->channels; i++) {
		vagLen[i] = rse->channelData[i*2];
		vagData[i] = (void*)((char*)buf + rse->channelData[i*2+1]);
	}
	
	ret = vag2wav(dest, rse->channels, vagLen, vagData);
	free(vagData);
	free(vagLen);
	return ret;
}

Bool dump_output_gimconv(char* dest, void* buf, rRCOEntry* entry, void* arg) {
#ifdef WIN32
	char tmpName[255];
	static uint successes = 0, failures = 0;
	RcoDumpGimconvOpts* opts = (RcoDumpGimconvOpts*)arg;
	Bool ret;
	
	if(failures <= 5 || successes > 0) {
		
		get_temp_fname(tmpName, "gim");
		
		FILE* fp = fopen(tmpName, "wb");
		if(!fp) return FALSE;
		
		// try to apply gim patch
		if(entry->srcLenUnpacked > 0x28) {
			// dirty code - we'll leave here, since the internal converter will have nicer code :P
			uint32* i32;
			Bool es = FALSE;
			i32 = (uint32*)buf;
			if(*i32 == 0x4D49472E) es = TRUE; // .GIM
			
			if(*i32 == 0x2E47494D || *i32 == 0x4D49472E) {
				uint16 i, i2;
				i = *(uint16*)((char*)buf + 0x10);
				i2 = *(uint16*)((char*)buf + 0x20);
				if(es) {
					i = ENDIAN_SWAP(i);
					i2 = ENDIAN_SWAP(i2);
				}
				if(i == 2 && i2 == 3) {
					uint32 sz = *(uint32*)((char*)buf + 0x14), sz2;
					i32 = (uint32*)((char*)buf + 0x24);
					sz2 = *i32;
					if(es) {
						sz = ENDIAN_SWAP(sz);
						sz2 = ENDIAN_SWAP(sz2);
					}
					if(sz-0x10 != sz2) {
						info("Note: Applied GIM patch when using GimConv to dump '%s'.", dest);
						sz2 = sz-0x10;
						if(es) sz2 = ENDIAN_SWAP(sz2);
						*i32 = sz2;
					}
				}
			}
		}
		
		filewrite(fp, buf, entry->srcLenUnpacked);
		fclose(fp);
		
		
		ret = exec_gimconv(opts->cmd, tmpName, dest, opts->extFlags);
		remove(tmpName);
		
		if(!ret) {
			warning("gimconv failed to process GIM.  Resource being dumped as GIM instead.");
			failures++;
			if(failures > 5 && successes == 0) {
				warning("GimConv failed too many times without success - disabling GimConv (GIMs will not be converted).");
			}
		}
	}
	else
		ret = FALSE;
	
	if(!ret) {
		char tmpDest[260];
		strcpy(tmpDest, dest);
		char* dot = strrchr(tmpDest, '.');
		if(!dot) dot = tmpDest + strlen(tmpDest);
		strcpy(dot, ".gim");
		
		return dump_output_data(tmpDest, buf, entry, NULL);
	} else
		successes++;
	return ret;
#else
	return FALSE;
#endif
}

Bool dump_output_vsmxdec(char* dest, void* buf, rRCOEntry* entry, void* arg) {
	VsmxMem* vm = readVSMXMem(buf);
	FILE* fp = openwrite(dest);
	if(fp) {
		if(VsmxDecode(vm, fp)) {
			warning("Unable to decode VSMX.  Dumping as data instead.");
			freeVsmxMem(vm);
			fclose(fp);
			return dump_output_data(dest, buf, entry, arg);
		}
		freeVsmxMem(vm);
		fclose(fp);
		return TRUE;
	}
	warning("Unable to open to file '%s'.", dest);
	return FALSE;
}

void dump_resources(char* labels, rRCOEntry* parent, const RcoTableMap extMap, char* pathPrefix, void* outputFilterArg) {
	// TODO: remove text crap from this function
	if(!parent || !parent->numSubentries) return;
	
	char fullOutName[MAX_FILENAME_LEN];
	uint extMapLen = 0;
	char dat[5] = "dat";
	
	strcpy(fullOutName, pathPrefix);
	char* outName = fullOutName + strlen(pathPrefix);
	
	while(extMap[extMapLen][0]) extMapLen++;
	
	#define MAX_LABEL_LEN 216
	uint i;
	rRCOEntry* entry;
	for(entry=parent->firstChild; entry; entry=entry->next) {
		char* ext = (char*)dat;
		if(entry->id == RCO_TABLE_IMG || entry->id == RCO_TABLE_MODEL) {
			uint fmt = ((rRCOImgModelEntry*)entry->extra)->format;
			if(fmt == RCO_IMG_GIM && outputFilterArg) {
				ext = ((RcoDumpGimconvOpts*)outputFilterArg)->ext;
			}
			else if(fmt <= extMapLen)
				ext = (char*)extMap[fmt];
		} else if(entry->id == RCO_TABLE_SOUND) {
			rRCOSoundEntry* rse = (rRCOSoundEntry*)entry->extra;
			if(rse->format == RCO_SOUND_VAG) {
				if(outputFilterArg)
					strcpy(ext, "wav");
				else
					strcpy(ext, "vag");
			}
		}
		
		
		char* label = get_label_from_offset(labels, entry->labelOffset);
		
		uint len = strlen(label);
		if(len > MAX_LABEL_LEN) len = MAX_LABEL_LEN;
		memcpy(outName, label, len);
		outName[len] = '.';
		
		OutputDumpFunc of = dump_output_data;
		if(outputFilterArg) {
			if(entry->id == RCO_TABLE_SOUND && ((rRCOSoundEntry*)entry->extra)->format == RCO_SOUND_VAG)
				of = dump_output_wav;
			else if(entry->id == RCO_TABLE_IMG && ((rRCOImgModelEntry*)entry->extra)->format == RCO_IMG_GIM)
				of = dump_output_gimconv;
		}
		
		info("Dumping resource '%s'...", label);
		
		if(entry->id == RCO_TABLE_SOUND && !outputFilterArg) {
			rRCOSoundEntry* rse = (rRCOSoundEntry*)entry->extra;
			char soundSetSrc[MAX_FILENAME_LEN] = "\0";
			for(i=0; i<rse->channels; i++) {
				outName[len+1] = '\0';
				if(!soundSetSrc[0]) {
					strcpy(soundSetSrc, fullOutName);
					strcpy(soundSetSrc + strlen(soundSetSrc), "ch*.vag");
				}
				
				sprintf(outName + len +1, "ch%d.", i);
				//len = strlen(outName) -1;
				
				strcpy(outName + strlen(outName), ext);
				
				uint origAddr = entry->srcAddr, origLen = entry->srcLen, origLenUnpacked = entry->srcLenUnpacked;
				entry->srcLen = entry->srcLenUnpacked = ((rRCOSoundEntry*)entry->extra)->channelData[i*2];
				entry->srcAddr += ((rRCOSoundEntry*)entry->extra)->channelData[i*2+1];
				if(!dump_resource(fullOutName, entry, of, outputFilterArg))
					warning("Unable to dump resource '%s'.", label);
				entry->srcAddr = origAddr;
				entry->srcLen = origLen;
				entry->srcLenUnpacked = origLenUnpacked;
			}
			strcpy(entry->srcFile, soundSetSrc);
		} else {
			strcpy(outName + len +1, ext);
			if(!dump_resource(fullOutName, entry, of, outputFilterArg))
				warning("Unable to dump resource '%s'.", label);
			
			// *new* fix entry here as well
			strcpy(entry->srcFile, fullOutName);
			entry->srcLenUnpacked = filesize(fullOutName); // get around issues of conversions changing filesize :P (dirty, but works!)
		}
		
		/*
		for(j=0; j<numResToWrite; j++) {
			if(entry->id == RCO_TABLE_SOUND) {
				// do something dirty - hack the srcFile here...
				outName[len+1] = '\0';
				strcpy(soundSetSrc, fullOutName);
				strcpy(soundSetSrc + strlen(soundSetSrc), "ch*.vag");
				
				sprintf(outName + len +1, "ch%d.", j);
				len = strlen(outName) -1;
			}
			strcpy(outName + len +1, ext);
			
			// trick dump_resource() into doing what we want it to by fiddling with stuff
			uint origAddr = entry->srcAddr, origLen = entry->srcLen, origLenUnpacked = entry->srcLenUnpacked;
			if(entry->id == RCO_TABLE_SOUND) {
				entry->srcLen = entry->srcLenUnpacked = ((rRCOSoundEntry*)entry->extra)->channelData[j*2];
				entry->srcAddr += ((rRCOSoundEntry*)entry->extra)->channelData[j*2+1];
			}
			
			if((fp = fopen(fullOutName, "wb"))) {
				if(!dump_resource(fp, entry))
					warning("Unable to dump resource '%s'.", labels + entry->labelOffset);
				fclose(fp);
			} else
				warning("Unable to write to file '%s'.", fullOutName);
			
			entry->srcAddr = origAddr;
			entry->srcLen = origLen;
			entry->srcLenUnpacked = origLenUnpacked;
			
			// *new* fix entry here as well
			if(entry->id != RCO_TABLE_SOUND)
				strcpy(entry->srcFile, fullOutName);
		}
		*/
		entry->srcAddr = 0;
		entry->srcLen = entry->srcLenUnpacked;
		entry->srcCompression = RCO_DATA_COMPRESSION_NONE;
	}
}

void dump_text_resources(char* labels, rRCOEntry* parent, Bool writeHeader, char* pathPrefix, Bool bWriteXML) {
	if(!parent || !parent->numSubentries) return;
	
	FILE* fp;
	char fullOutName[MAX_FILENAME_LEN];
	
	
	#define MAX_LABEL_LEN 216
	uint i;
	rRCOEntry* entry;
	for(entry=parent->firstChild; entry; entry=entry->next) {
		// read & decompress text data
		char* textBuffer;
		rRCOTextEntry* textEntry = ((rRCOTextEntry*)entry->extra);
		if(!(fp = fopen(entry->srcFile, "rb"))) return;
		if(entry->srcAddr) fseek(fp, entry->srcAddr, SEEK_SET);
		char* inBuffer = (char*)malloc(entry->srcLen);
		fileread(fp, inBuffer, entry->srcLen);
		fclose(fp);
		
		strcpy(fullOutName, pathPrefix);
		char* outName = fullOutName + strlen(pathPrefix);
		rcoxml_int_to_text(textEntry->lang, RCOXML_TABLE_TEXT_LANG, outName);
		// dirty, but since stuff is here conveniently, update srcFile
		info("Dumping %s text entries...", outName);
		if(bWriteXML) {
			sprintf(entry->srcFile, "%s.xml", fullOutName);
			outName += strlen(outName);
			strcpy(outName, ".xml");
		}
		else {
			makedir(fullOutName);
			sprintf(entry->srcFile, "%s%c*.txt", fullOutName, DIR_SEPARATOR);
			outName += strlen(outName);
			outName[0] = DIR_SEPARATOR;
			outName++;
		}
		
		if(entry->srcCompression) {
			textBuffer = (char*)malloc(entry->srcLenUnpacked);
			if(entry->srcCompression == RCO_DATA_COMPRESSION_ZLIB) {
				int uRet = zlib_uncompress(textBuffer, entry->srcLenUnpacked, inBuffer, entry->srcLen);
				if(uRet != Z_OK && uRet != Z_DATA_ERROR) return;
				if(uRet == Z_DATA_ERROR)
					warning("Encountered 'data error' when decompressing text resource.");
			}
			free(inBuffer);
		} else
			textBuffer = inBuffer;
		
		iconv_t ic = NULL; // prevent gcc warning
		if(bWriteXML) {
			if(!(fp = fopen(fullOutName, "wb"))) {
				warning("Unable to write to file '%s'.", fullOutName);
				continue;
			}
			// put UTF-8 header
			fputc(0xEF, fp);
			fputc(0xBB, fp);
			fputc(0xBF, fp);
			fputs("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n", fp);
			fprintf(fp, "<!-- This XML was generated by %s -->\n", APPNAME_VER);
			fputs("<TextLang>\n", fp);
			
			// set up iconv
			char icSrctype[8];
			make_iconv_charset(icSrctype, textEntry->format, parent->rco->eSwap);
			ic = iconv_open("utf-8", icSrctype);
		}
		
		for(i=0; i<textEntry->numIndexes; i++) {
			RCOTextIndex* idx = &(textEntry->indexes[i]);
			uint len = strlen(get_label_from_offset(labels, idx->labelOffset));
			uint dataLen = 0;
			if(len > MAX_LABEL_LEN) len = MAX_LABEL_LEN;
			
			if(idx->length) {
				uint charWidth = RCO_TEXT_FMT_CHARWIDTH(textEntry->format);
				dataLen = idx->length;
				if(idx->length >= charWidth) {
					int j;
					for(j=0; j<(int)charWidth; j++) {
						if(textBuffer[idx->offset + idx->length - (j+1)]) break;
					}
					if(j == (int)charWidth)
						dataLen -= charWidth;
				}
			}
			
			if(!bWriteXML) {
				memcpy(outName, get_label_from_offset(labels, idx->labelOffset), len);
				strcpy(outName + len, ".txt");
				
				if(!(fp = fopen(fullOutName, "wb"))) {
					warning("Unable to write to file '%s'.", fullOutName);
					continue;
				}
				if(writeHeader) {
					if(textEntry->format == RCO_TEXT_FMT_UTF32) {
						uint32 bom = UTF32_BOM;
						if(parent->rco->eSwap) bom = ENDIAN_SWAP(bom);
						filewrite(fp, &bom, sizeof(bom));
					} else if(textEntry->format == RCO_TEXT_FMT_UTF8) {
						uint32 bom = UTF8_BOM;
						filewrite(fp, &bom, 3);
					} else {
						uint16 bom = UTF16_BOM;
						if(parent->rco->eSwap) bom = ENDIAN_SWAP(bom);
						filewrite(fp, &bom, sizeof(bom));
					}
				}
				if(dataLen)
					filewrite(fp, textBuffer + idx->offset, dataLen);
				
				fclose(fp);
			}
			else {
				char* bufIn = textBuffer + idx->offset;
				Bool useCdata = (memchr(bufIn, '<', dataLen) || memchr(bufIn, '>', dataLen) || memchr(bufIn, '&', dataLen));
				fprintf(fp, "\t<Text name=\"%s\">", get_label_from_offset(labels, idx->labelOffset));
				if(useCdata)
					fputs("<![CDATA[", fp);
				
				char buf[4096];
				char* bufOut = buf;
				uint outBufLen = 4096;
				/* {
					// feed in the BOM (is it really necessary though?)
					uint number;
					char* unicodePtr;
					if(textEntry->format == RCO_TEXT_FMT_UTF32) {
						uint32 bom = UTF32_BOM;
						if(parent->rco->eSwap) bom = ENDIAN_SWAP(bom);
						number = sizeof(bom);
						unicodePtr = (char*)&bom;
						iconv(ic, (const char**)(&unicodePtr), (size_t*)(&number), &bufOut, (size_t*)(&outBufLen));
					} else if(textEntry->format == RCO_TEXT_FMT_UTF8) {
						uint32 bom = UTF8_BOM;
						number = 3;
						unicodePtr = (char*)&bom;
						iconv(ic, (const char**)(&unicodePtr), (size_t*)(&number), &bufOut, (size_t*)(&outBufLen));
					} else {
						uint16 bom = UTF16_BOM;
						if(parent->rco->eSwap) bom = ENDIAN_SWAP(bom);
						number = sizeof(bom);
						unicodePtr = (char*)&bom;
						iconv(ic, (const char**)(&unicodePtr), (size_t*)(&number), &bufOut, (size_t*)(&outBufLen));
					}
				} */
				uint nullsStripped = 0;
				while(dataLen) {
					iconv(ic, (&bufIn), (size_t*)(&dataLen), &bufOut, (size_t*)(&outBufLen));
					if(buf == bufOut) {
						warning("iconv failed when converting resource '%s'.", get_label_from_offset(labels, idx->labelOffset));
						break;
					}
					// strip null characters - UTF-8's encoding scheme doesn't contain null bytes in itself, so this simple solution works nicely :)
					char* bufTmp;
					for(bufTmp=buf; bufTmp<bufOut; bufTmp++) {
						if(*bufTmp)
							fputc(*bufTmp, fp);
						else
							nullsStripped++;
					}
					outBufLen = 4096;
					bufOut = buf;
					/*
					filewrite(fp, buf, bufOut-buf);
					outBufLen = 4096;
					bufOut = buf;
					*/
				}
				if(nullsStripped)
					warning("%d null(s) were stripped from resource '%s'.", nullsStripped, get_label_from_offset(labels, idx->labelOffset));
				
				if(useCdata)
					fputs("]]>", fp);
				fputs("</Text>\n", fp);
			}
			
		}
		if(bWriteXML) {
			fputs("</TextLang>\n", fp);
			fclose(fp);
			iconv_close(ic);
		}
		free(textBuffer);
	}
}

void compile_gimconv_map(rRCOFile* rco, rRCOEntry* entry, void* arg) {
	if(entry->id != RCO_TABLE_IMG || entry->type != 1 || ((rRCOImgModelEntry*)entry->extra)->format != RCO_IMG_GIM) return;
	
	if(entry->srcFile[0] && entry->srcAddr == 0 && entry->srcLen == filesize(entry->srcFile) && entry->srcCompression == RCO_DATA_COMPRESSION_NONE && strcasecmp(entry->srcFile + strlen(entry->srcFile) - 4, ".gim")) {
		info("Converting '%s' to GIM...", get_label_from_offset(rco->labels, entry->labelOffset));
		RcoDumpGimconvOpts* opts = (RcoDumpGimconvOpts*)arg;
		char tmpName[255];
		get_temp_fname(tmpName, "gim");
		char gimconvSwpCmd[255] = "-ps3";
		char* extFlags = opts->extFlags;
		if(rco->eSwap) {
			extFlags = gimconvSwpCmd;
			if(opts->extFlags) {
				strcat(extFlags, " ");
				strcat(extFlags, gimconvSwpCmd);
			}
		}
		if(exec_gimconv(opts->cmd, entry->srcFile, tmpName, extFlags)) {
			FILE* fp = fopen(tmpName, "rb");
			if(fp) {
				entry->srcLen = entry->srcLenUnpacked = filesize(tmpName);
				entry->srcFile[0] = '\0';
				entry->srcBuffer = malloc(entry->srcLen);
				entry->srcCompression = RCO_DATA_COMPRESSION_NONE;
				fileread(fp, entry->srcBuffer, entry->srcLen);
				fclose(fp);
				remove(tmpName);
			}
			else
				warning("Open '%s' failed.", tmpName);
		} else
			warning("Failed to convert resource '%s' to GIM.", get_label_from_offset(rco->labels, entry->labelOffset));
	}
}

void compile_vagconv_map(rRCOFile* rco, rRCOEntry* entry, void* arg) {
	rRCOSoundEntry* rse = (rRCOSoundEntry*)entry->extra;
	if(entry->id != RCO_TABLE_SOUND || entry->type != 1 || rse->format != RCO_SOUND_VAG) return;
	
	if(entry->srcFile[0] && entry->srcAddr == 0 && entry->srcCompression == RCO_DATA_COMPRESSION_NONE && !strcasecmp(entry->srcFile + strlen(entry->srcFile) - 4, ".wav")) {
		uint len;
		int i;
		
		rse->channels = wav2vag(entry->srcFile, &len, &entry->srcBuffer, "");
		if(!rse->channels) {
			error("WAV->VAG conversion failed for '%s'", get_label_from_offset(rco->labels, entry->labelOffset));
			return;
		}
		entry->srcFile[0] = '\0';
		entry->srcLen = entry->srcLenUnpacked = len * rse->channels;
		entry->srcCompression = RCO_DATA_COMPRESSION_NONE;
		
		if(!rse->channelData)
			rse->channelData = (uint32*)malloc(rse->channels * sizeof(uint32)*2);
		for(i=0; i<rse->channels; i++) {
			rse->channelData[i*2] = len;
			rse->channelData[i*2+1] = len*i;
		}
	}
}

void compile_vsmxconv_map(rRCOFile* rco, rRCOEntry* entry, void* arg) {
	if(entry->id != RCO_TABLE_VSMX || entry->type != 1) return;
	if(entry->srcFile[0] && entry->srcAddr == 0 && entry->srcCompression == RCO_DATA_COMPRESSION_NONE && !strcasecmp(entry->srcFile + strlen(entry->srcFile) - 4, ".txt")) {
		FILE* fin = fopen(entry->srcFile, "rb");
		if(fin) {
			VsmxMem* vm = VsmxEncode(fin);
			if(vm) {
				entry->srcBuffer = writeVSMXMem(&entry->srcLen, vm);
				entry->srcLenUnpacked = entry->srcLen;
				entry->srcFile[0] = '\0';
				entry->srcCompression = RCO_DATA_COMPRESSION_NONE;
				freeVsmxMem(vm);
			} else
				error("Could not encode VSMX!");
			fclose(fin);
		}
	}
}

void compile_wavcheck_map(rRCOFile* rco, rRCOEntry* entry, void* arg) {
	if(entry->id == RCO_TABLE_SOUND && entry->type == 1 && ((rRCOSoundEntry*)entry->extra)->channels == 0)
		warning("Unable to determine channel data for sound '%s'.", get_label_from_offset(rco->labels, entry->labelOffset));
}

Bool exec_gimconv(char* cmd, char* src, char* dest, char* extFlags) {
#ifdef WIN32
	char gimconvCmd[1200];
	int ret;
	
	if(cmd)
		sprintf(gimconvCmd, "\"%s\"", cmd);
	else
		strcpy(gimconvCmd, "gimconv");
	
	if(extFlags) {
		sprintf(gimconvCmd + strlen(gimconvCmd), " %s", extFlags);
	}
	
	// gimconv is screwy and sometimes prepends a '/' to our destination file
	// so we do our feeble attempt to determine if it is a relative path
	if(dest[0] == '\\' || dest[0] == '/' || dest[0] == '.' || dest[1] == ':') // should handle "\file", "\\network\blah" and "C:\file" notations; gimconv seems to also escape if first char == '.'
		sprintf(gimconvCmd + strlen(gimconvCmd), " \"%s\" -o \"%s\"", src, dest);
	else
		sprintf(gimconvCmd + strlen(gimconvCmd), " \"%s\" -o \"./%s\"", src, dest);
	
	{
		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		DWORD exitCode;
		HANDLE hNull = CreateFileA("NUL", FILE_APPEND_DATA, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
		si.hStdOutput = hNull;
		ZeroMemory(&pi, sizeof(pi));
		
		if(!CreateProcessA(NULL, gimconvCmd, NULL, NULL, FALSE, 0, NULL, NULL, (LPSTARTUPINFOA)&si, &pi))
			return FALSE;
		
		if(WaitForSingleObject(pi.hProcess, 5000) == WAIT_TIMEOUT) {
			TerminateProcess(pi.hProcess, 1);
			exitCode = 1; //failed
			warning("GimConv wait time exceeded - process automatically terminated.");
		} else
			GetExitCodeProcess(pi.hProcess, (LPDWORD)&exitCode);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		CloseHandle(hNull);
		ret = (int)exitCode;
	}
	
	//ret = system(gimconvCmd);
	return (ret == 0);
#else
	return FALSE;
#endif
}

