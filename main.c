
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "strfuncs.h"
#include "general.h"
#include "rcomain.h"
#include "xml.h"
#include "rcodump.h"
#include "vaghandler.h"
#include "vsmx.h"
#include "configscan.h"

#define MAIN_INV_CMD_SYNTAX { \
	error("Invalid command syntax. See '%s help %s' for help.", app_argv[0], app_argv[1]); \
	return RETERR_SYNTAX; \
}


// "1" is reserved
#define RETERR_SYNTAX		2
#define RETERR_READRCO		3
#define RETERR_WRITEXML		4
#define RETERR_READXML		5
#define RETERR_WRITERCO		6
#define RETERR_GENERIC_FAILED	10

int main_help(void);
int main_dump(void);
int main_compile(void);

int main_rebuild(void);

int main_extract(void);
//int main_replace(void);
int main_vagdec(void);
int main_vagenc(void);
int main_vsmxdec(void);
int main_vsmxenc(void);


void retrieve_from_opts(Bool warnUnk, int num, ...);

Bool quietMode;
int app_argc;
char** app_argv;

extern Bool suppressDecompWarnings;

int main(int argc, char** argv) {
	app_argc = argc;
	app_argv = argv;
	
	if(argc < 2 || !strcasecmp(app_argv[1], "help")) {
		int ret = main_help();
		// we'll assume this is executed not from the cmd line so we'll call a pause
		//system("pause");
		return ret;
	}
	
	int (*func)(void) = NULL;
	
	     if(!strcasecmp(app_argv[1], "dump"))
		func = main_dump;
	else if(!strcasecmp(app_argv[1], "compile"))
		func = main_compile;
	else if(!strcasecmp(app_argv[1], "rebuild"))
		func = main_rebuild;
	else if(!strcasecmp(app_argv[1], "extract"))
		func = main_extract;
	//else if(!strcasecmp(app_argv[1], "replace"))
	//	func = main_replace;
	else if(!strcasecmp(app_argv[1], "vagdec"))
		func = main_vagdec;
	else if(!strcasecmp(app_argv[1], "vagenc"))
		func = main_vagenc;
	else if(!strcasecmp(app_argv[1], "vsmxdec"))
		func = main_vsmxdec;
	else if(!strcasecmp(app_argv[1], "vsmxenc"))
		func = main_vsmxenc;
	else {
		error("Unknown function '%s'. Use '%s help' to see available functions.", app_argv[1], app_argv[0]);
		return RETERR_SYNTAX;
	}
	
	retrieve_from_opts(FALSE, 2,
		"--quiet",		"bool", &quietMode,
		"--ini-dir",	"string", &configDir
	);
	
	info("%s, written by ZiNgA BuRgA\nA general purpose RCO creation and manipulation command-line tool.\n", APPNAME_VER);
	
	configLoadTagmap();
	configLoadMiscmap();
	
	return func();
}


int main_help() {
	printf("%s, written by ZiNgA BuRgA\nA general purpose RCO creation and manipulation command-line tool.\n\n", APPNAME_VER);
	
	
	#ifdef DISABLE_RLZ
	#define PRINT_PACK_OPTS \
		printf( \
			"  For the following --pack-* functions, values can be 'none' or 'zlib'.\n" \
			"  --pack-res and --pack-cmp, if specified, override values stored in XML.\n" \
			"    --pack-hdr <val>  How to compress the RCO header. [none]\n" \
			"    --pack-res <val>  How to compress RCO resources (BMP, GIM & GMO).\n" \
			"    --pack-cmp <val>  Compression used on already compressed resources. [none]\n" \
			"                      This can be used to force additional compression on PNG,\n" \
			"                      JPEG, TIFF and GIF resources. 'none' is recommended.\n" \
			"    --zlib-method <val>\n" \
			"                      Zlib compression method/strategy [7z]\n" \
			"                      Can be default, filtered, huffman, rle, fixed or 7z\n" \
			"                      '7z' will use 7-Zip's deflate instead of zlib\n" \
			"    --zlib-level <n>  Zlib compression level [3]\n" \
			"                      Values can be 0-9, or 1-4 for '--zlib-method 7z'\n" \
			"                      Defaults to 9 if not using 7z\n" \
		)
	#else
	#define PRINT_PACK_OPTS \
		printf( \
			"  For the following --pack-* functions, values can be 'none', 'zlib' or 'rlz'.\n" \
			"  --pack-res and --pack-cmp, if specified, override values stored in XML.\n" \
			"  Note: RLZ compression is EXPERIMENTAL!\n" \
			"    --pack-hdr <val>  How to compress the RCO header. [none]\n" \
			"    --pack-res <val>  How to compress RCO resources (BMP, GIM & GMO).\n" \
			"    --pack-cmp <val>  Compression used on already compressed resources. [none]\n" \
			"                      This can be used to force additional compression on PNG,\n" \
			"                      JPEG, TIFF and GIF resources. 'none' is recommended.\n" \
			"    --zlib-method <val>\n" \
			"                      Zlib compression method/strategy [7z]\n" \
			"                      Can be default, filtered, huffman, rle, fixed or 7z\n" \
			"                      '7z' will use 7-Zip's deflate instead of zlib\n" \
			"    --zlib-level <n>  Zlib compression level [3]\n" \
			"                      Values can be 0-9, or 1-4 for '--zlib-method 7z'\n" \
			"                      Defaults to 9 if not using 7z\n" \
			"    --rlz-mode <n>    RLZ compression mode [-1]\n" \
			"                      Values can be 0-31, or -1\n" \
			"                      -1 tries modes 5, 6 & 7 and selects optimal output (default\n" \
			"                      Sony behaviour)\n" \
		)
	#endif
	
	
	if(app_argc > 2) {
		if(!strcasecmp(app_argv[2], "dump")) {
			printf("Syntax: %s dump <rcofile> <xmlfile> [<resource_dirs>] [options]\n", app_argv[0]);
			printf(
				"  Dumps the structure of <rcofile> in an XML format to <xmlfile>.\n"
				"  <xmlfile> can be '-' which means stdout.\n"
				"\n"
				"\nOptions:\n"
				"    --resdir <dir> Folder to dump resources into, or '-' for no dumping.\n"
				"                   Resources will be dumped to <dir>, but you can have custom\n"
				"                   directories for different resources with the following:\n"
				"                     --images <dir>\n"
				"                     --sounds <dir>\n"
				"                     --models <dir>\n"
				"                     --text <dir>\n"
				"                     --vsmx <dir>\n"
				"                   You can also use '-' as <dir> for the above to disable\n"
				"                   dumping resources of that type.\n"
				"    --output-txt   Output separate .txt files for every text string. Each is\n"
				"                   prepended with the appropriate UCS BOM.\n"
				"    --conv-gim <ext>\n"
				"                   Send GIM images through gimconv (Windows only) and\n"
				"                   convert to type with specified extension (ie png, bmp etc)\n"
				"    --gimconv-cmd <command>\n"
				"                   gimconv command to execute; defaults to 'gimconv'.\n"
				"    --gimconv-flags <flags>\n"
				"                   Additional flags to pass to gimconv.\n"
				"    --conv-vag     Convert VAG files to WAV.\n"
				"    --decode-vsmx  Decode VSMX/JSX files to textual format.\n"
				"    --no-decomp-warn\n"
				"                   Suppress decompression warnings.\n"
				"\n"
				"\nNote: for resource dumping, directories are NOT automatically created. If the\n"
				"  specified directorie(s) don't exist, the dumping will fail. However,\n"
				"  directories for text languages with the '--output-txt' option will be\n"
				"  automatically created if necessary.\n"
			);
			return 0;
		}
		else if(!strcasecmp(app_argv[2], "compile")) {
			printf("Syntax: %s compile <xmlfile> <rcofile> [options]\n", app_argv[0]);
			printf(
				"  Compiles an RCO <rcofile> using structure defined in <xmlfile>.\n"
				"  <xmlfile> can be '-' which means stdin.\n"
				"  Note that the XML file may have linked resources which need to be present for\n"
				"  the compilation process to succeed.\n"
				"\n"
				"\nOptions:\n"
			);
			PRINT_PACK_OPTS;
			printf("\n"
				"    --no-convgim      Don't automatically run images marked as format=gim\n"
				"                      through gimconv if extension is not '.gim'.\n"
				"    --gimconv-cmd <command>\n"
				"                      gimconv command to execute; defaults to 'gimconv'.\n"
				"    --gimconv-flags <flags>\n"
				"                      Additional flags to pass to gimconv.\n"
				"    --no-convvag      Don't automatically convert WAV sounds to VAG format\n"
				"                      (based on extension).  Note WAV->VAG conversion is lossy!\n"
				"    --no-encvsmx      Don't automatically encode text files to VSMX\n"
				"                      (based on extension).\n"
			);
			return 0;
		}
		else if(!strcasecmp(app_argv[2], "rebuild")) {
			printf("Syntax: %s rebuild <rco_in> <rco_out> [options]\n", app_argv[0]);
			printf(
				"  Simply rebuilds an RCO <rco_in>, writing out to <rco_out>; useful for\n"
				"  changing compression used.\n"
				"\n"
				"\nOptions:\n"
			);
			PRINT_PACK_OPTS;
			return 0;
		}
		
		else if(!strcasecmp(app_argv[2], "extract")) {
			printf("Syntax: %s extract <rcofile> <resource> [<output>] [options]\n", app_argv[0]);
			printf("  Extracts a single resource (image/sound/model/VSMX/text) with label\n"
			 "  <resource> and saves it to <output>.  If <output> is not specified, will default\n"
			 "  to using the label as the filename, with no extension.  <output> can be '-'\n"
			 "  meaning stdout.\n"
			 "   * You should supply the '--lang' option when extracting text resources.\n"
			 "   * You should supply the '--channel' option when extracting sound resources.\n"
			 "\n"
			 "\nOptions:\n"
			 "  The following options only apply for extracting text resources.\n"
			 "    --lang <lang>     Language of text to extract. [English]\n"
			 "                      You can use a language ID or one of the following:\n");
			uint i=0;
			while(RCOXML_TABLE_TEXT_LANG[i][0]) {
				printf("                       - %s (ID=%d)\n", RCOXML_TABLE_TEXT_LANG[i], i);
				i++;
			}
			printf("    --no-txt-hdr      Don't write UCS BOM.\n"
			 "  The following option only applies for extracting sound resources.\n"
			 "    --channel <n>     Extract sound channel <n>. [1]\n"
			 " ** Note, --channel has currently not been implemented; will dump all channels.\n");
			return 0;
		}
		/* else if(!strcasecmp(app_argv[2], "replace")) {
			printf("Syntax: %s replace <rcofile> <resource> <file> [<outrco>] [options]\n", app_argv[0]);
			printf(
			 "  Replaces a single resource (image/sound/model/VSMX/text) in <rcofile> with\n"
			 "  label <resource> with that stored in <file>, and rebuilds RCO as <outrco>.\n"
			 "  If <outrco> is not supplied, will overwrite <rcofile> with this rebuilt RCO.\n"
			 "   * You should supply the '--lang' option when replacing text resources.\n"
			 "   * You should supply the '--channel' option when replacing sound resources.\n"
			 "\n"
			 "\nOptions:\n"
			 "    --lang <lang>     Language of text to replace. [English]\n"
			 "                      See '%s help extract' for valid values.\n"
			 "    --channel <n>     Replace sound channel <n>. [1]\n"
			 "    --format <fmt>    Format of imported resource.  Only applies to image,\n"
			 "                      sound and model resources.  You can use a format ID, or\n"
			 "                      the following:\n");
			// TODO: 
			printf(
			 "    --pack <val>      How to compress this resource.\n"
			#ifdef DISABLE_RLZ
			 "                      Valid values are 'none' and 'zlib'.\n"
			#else
			 "                      Valid values are 'none', 'zlib' and 'rlz'.\n"
			#endif
			 "                      Only applies to images and models.  By default, will use\n"
			 "                      'zlib' unless format is PNG/JPG/TIF/GIF where 'none' will\n"
			 "                      be used.\n");
			PRINT_PACK_OPTS;
			return 0;
		} */
		/*
		else if(!strcasecmp(app_argv[2], "list")) {
			printf("Syntax: %s list <rcofile> [options]\n", app_argv[0]);
			printf("  List resources in an easy to parse format.\n");
			printf("\n");
			printf("\nOptions:\n");
			printf("    --type <type>     Only list resources of a certain type.\n");
			printf("                      Types can be 'image', 'sound', 'model', \n");
			printf("    --no-recurse      Don't write UCS BOM.\n");
			printf("  The following option only applies for extracting sound resources.\n");
			printf("    --channel <n>     Extract sound channel <n>. [1]\n");
			return 0;
		}
		*/
		
		else if(!strcasecmp(app_argv[2], "vagdec")) {
			printf("Syntax: %s vagdec <vag.ch1> [<vag.ch2> [<vag.ch3> ...]] <wav>\n", app_argv[0]);
			printf(
			 "  Converts input VAG files to WAV file <wav>, where each input file is a\n"
			 "  separate channel.  Can use '-' for VAG or WAV files which mean stdin or\n"
			 "  stdout respectively.\n"
			);
			return 0;
		}
		else if(!strcasecmp(app_argv[2], "vagenc")) {
			printf("Syntax: %s vagenc <wav> <vag.ch1> [<vag.ch2> [<vag.ch3> ...]]\n", app_argv[0]);
			printf(
			 "  Converts input WAV file <wav> to VAG.  Output will be a separate file for each\n"
			 "  channel.  If only one output VAG is specified and input WAV has multiple\n"
			 "  channels, will automatically append extensions.  Input/outputs can be '-'\n"
			 "  which mean stdin/stdout."
			);
			return 0;
		}
		else if(!strcasecmp(app_argv[2], "vsmxdec")) {
			printf("Syntax: %s vsmxdec [--decompile] <vsmxfile> <output>\n", app_argv[0]);
			printf(
			 "  Decodes the input <vsmxfile> to <output>, which can be '-' meaning stdout.\n"
			 "  If --decompile option is specified, will activate experimental decompiler\n"
			 "  and will output decompiled Javascript.\n"
			);
			return 0;
		}
		else if(!strcasecmp(app_argv[2], "vsmxenc")) {
			printf("Syntax: %s vsmxenc <textfile> <vsmxfile>\n", app_argv[0]);
			printf(
			 "  Encodes input <textfile> to <vsmxfile>. <textfile> can be '-', meaning stdin.\n"
			 "  Note that this cannot compile Javascript, only decoded text files.\n"
			);
			return 0;
		}
	}
	
	printf("Syntax: %s <function> [options]\n", app_argv[0]);
	printf("  Use '%s help <function>' for help on a specific function\n", app_argv[0]);
	
	printf("\n");
	printf("\nAvailable functions:\n");
	printf("\n");
	//printf("    info           Display info on an RCO file or a resource contained within.\n");
	//printf("    list           List contents/resources in an RCO file.\n");
	printf("    extract        Extract a resource from an RCO file.\n");
	printf("    dump           Dumps structure and contents of an RCO file.\n");
	printf("\n");
	printf("    compile        Compiles a new RCO file from a dump.\n");
	printf("    rebuild        Reads an RCO, then writes it out; in other words, it just\n");
	printf("                   copies it (and doesn't do it perfectly) - incredibly useful\n");
	printf("                   eh? ^^\n");
	//printf("    replace        Replace a resource in an RCO file.\n");
	printf("\n");
	printf("    vagdec         Converts VAG to WAV.\n");
	printf("    vagenc         Converts WAV to VAG.\n");
	printf("\n");
	printf("    vsmxdec        Decodes a VSMX/JSX file into a text file.\n");
	printf("    vsmxenc        Encodes a (VSMX) text file into a VSMX/JSX file.\n");
	printf("\n");
	printf("    help           This help screen.\n");
	printf("\n");
	printf("\nAvailable options:\n");
	printf("\n");
	printf("    --quiet        Only display warnings/errors.\n");
	printf("    --ini-dir      Specify directory containing Rcomage INI files.\n");
	
	/*
	printf("\n");
	printf("\nGeneral options:\n");
	printf("\n");
	printf("    --pack \n");
	*/
	
	//edit/show page/anim data
	
	
	return 0;
}

int main_dump(void) {
	if(app_argc < 4) MAIN_INV_CMD_SYNTAX;
	
	char* sRcoFile = NULL;
	char* sXmlFile = NULL;
	Bool sTextOutput = FALSE;
	//char curPath[] = ".";
	char* sResDir = NULL;
	char* sResImgDir = NULL;
	char* sResSndDir = NULL;
	char* sResMdlDir = NULL;
	char* sResVsmxDir = NULL;
	char* sResTxtDir = NULL;
	
	//char* sConvGim = NULL;
	RcoDumpGimconvOpts gimconvOpts;
	Bool sConvVag = FALSE;
	Bool sConvVsmx = FALSE;
	
	int i;
	gimconvOpts.ext = gimconvOpts.cmd = gimconvOpts.extFlags = NULL;
	// parse options
	retrieve_from_opts(TRUE, 13,
		"--output-txt",		"bool", &sTextOutput,
		"--conv-gim",		"text", &gimconvOpts.ext,
		"--gimconv-cmd",	"text", &gimconvOpts.cmd,
		"--gimconv-flags",	"text", &gimconvOpts.extFlags,
		"--conv-vag",		"bool", &sConvVag,
		"--decode-vsmx",	"bool", &sConvVsmx,
		"--resdir",			"text", &sResDir,
		"--images",			"text", &sResImgDir,
		"--sounds",			"text", &sResSndDir,
		"--models",			"text", &sResMdlDir,
		"--vsmx",			"text", &sResVsmxDir,
		"--text",			"text", &sResTxtDir,
		"--no-decomp-warn",	"bool", &suppressDecompWarnings
	);
	
#ifndef WIN32
	if(gimconvOpts.ext) {
		warning("gimconv support is only available on Windows, as stuff only gets compiled for that platform and Sony doesn't give away source code (duh).");
		gimconvOpts.ext = NULL;
	}
#endif
	
	for(i=2; i<app_argc; i++) {
		if(!app_argv[i]) continue;
		if(!sRcoFile)
			sRcoFile = app_argv[i];
		else if(!sXmlFile)
			sXmlFile = app_argv[i];
		else MAIN_INV_CMD_SYNTAX
	}
	if(!sRcoFile || !sXmlFile) MAIN_INV_CMD_SYNTAX
	
	if(gimconvOpts.ext && strlen(gimconvOpts.ext) > 5) {
		gimconvOpts.ext[5] = '\0'; // prevent buffer overflow
		if(gimconvOpts.cmd && strlen(gimconvOpts.cmd) > 255) {
			gimconvOpts.cmd[255] = '\0'; // prevent buffer overflow
		}
		if(gimconvOpts.extFlags && strlen(gimconvOpts.extFlags) > 512) {
			gimconvOpts.extFlags[512] = '\0'; // prevent buffer overflow
		}
	}
	
	rRCOFile* rco = read_rco(sRcoFile);
	if(!rco) {
		return RETERR_READRCO;
	}
	
	
	// dump resources
	char textPathPrefix[MAX_FILENAME_LEN] = "\0";
	Bool sndDumped = FALSE;
	if((!sResDir || strcmp(sResDir, "-")) && (rco->tblImage || rco->tblSound || rco->tblModel || rco->tblVSMX || rco->tblText)) {
		char pathPrefix[MAX_FILENAME_LEN] = "\0";
		#define RCO_PRINT_PATH(p, c) \
			if(sResDir && c) \
				sprintf(p, "%s%c%s%c", sResDir, DIR_SEPARATOR, c, DIR_SEPARATOR); \
			else if(sResDir) \
				sprintf(p, "%s%c", sResDir, DIR_SEPARATOR); \
			else if(c) \
				sprintf(p, "%s%c", c, DIR_SEPARATOR); \
			else \
				p[0] = '\0';
		
		if(!sResImgDir || strcmp(sResImgDir, "-")) {
			void* arg = (void*)&gimconvOpts;
			RCO_PRINT_PATH(pathPrefix, sResImgDir);
			if(!gimconvOpts.ext) arg = NULL;
			dump_resources(rco->labels, rco->tblImage, RCOXML_TABLE_IMG_FMT, pathPrefix, arg);
		}
		if(!sResSndDir || strcmp(sResSndDir, "-")) {
			RCO_PRINT_PATH(pathPrefix, sResSndDir);
			dump_resources(rco->labels, rco->tblSound, RCOXML_TABLE_SOUND_FMT, pathPrefix, (void*)sConvVag);
			sndDumped = TRUE;
		}
		if(!sResMdlDir || strcmp(sResMdlDir, "-")) {
			RCO_PRINT_PATH(pathPrefix, sResMdlDir);
			dump_resources(rco->labels, rco->tblModel, RCOXML_TABLE_MODEL_FMT, pathPrefix, NULL);
		}
		
		
		if(!sResTxtDir || strcmp(sResTxtDir, "-")) {
			RCO_PRINT_PATH(textPathPrefix, sResTxtDir);
			dump_text_resources(rco->labels, rco->tblText, TRUE, textPathPrefix, !sTextOutput);
		}
		
		
		if((!sResVsmxDir || strcmp(sResVsmxDir, "-")) && rco->tblVSMX) {
			rRCOEntry* rcoNode;
			for(rcoNode=rco->tblMain.firstChild; rcoNode; rcoNode=rcoNode->next) {
				if(rcoNode->id == RCO_TABLE_VSMX) {
					char defName[] = "vsmx";
					char* label = defName;
					if(rcoNode->labelOffset)
						label = rco->labels + rcoNode->labelOffset;
					RCO_PRINT_PATH(pathPrefix, sResVsmxDir);
					sprintf(pathPrefix + strlen(pathPrefix), (sConvVsmx ? "%s.txt" : "%s.vsmx"), label);
					info("Dumping VSMX resource '%s'...", label);
					if(!dump_resource(pathPrefix, rcoNode, (sConvVsmx ? dump_output_vsmxdec : dump_output_data), NULL)) {
						if(rcoNode->labelOffset) {
							warning("Unable to dump VSMX resource '%s'.", label);
						} else {
							warning("Unable to dump VSMX resource.");
						}
					}
					
					strcpy(rcoNode->srcFile, pathPrefix);
					rcoNode->srcAddr = 0;
					rcoNode->srcCompression = RCO_DATA_COMPRESSION_NONE;
				}
			}
		}
	}
	
	// hack for text resources if dumping is disabled
	if((sResDir && !strcmp(sResDir, "-")) || (sResTxtDir && !strcmp(sResTxtDir, "-")))
		sTextOutput = TRUE;
	
	// shouldn't need to check whether file exists cause we're not doing a binary write?
	
	FILE* fp;
	if(!strcmp(sXmlFile, "-"))
		fp = stdout;
	else if(!(fp = fopen(sXmlFile, "w"))) {
		error("Unable to open file %s", sXmlFile);
		return RETERR_WRITEXML;
	}
	
	int sndDumpVar = (sndDumped ? 1:0);
	if(sndDumped && sConvVag) sndDumpVar = 2;
	if(!write_xml(rco, fp, (textPathPrefix[0] ? textPathPrefix : NULL), !sTextOutput, sndDumpVar, sConvVsmx))
		return RETERR_WRITEXML;
	
	return 0;
}

int main_extract(void) {
	if(app_argc < 4) MAIN_INV_CMD_SYNTAX;
	
	char* sRcoFile = NULL;
	char* sResLabel = NULL;
	char* sOutFile = NULL;
	char* stTextLang = NULL;
	uint sTextLang = 1;
	Bool sTextNoHdr = FALSE;
	int sSndChannel = 1;
	
	int i;
	// parse options
	retrieve_from_opts(TRUE, 3,
		"--lang",			"text", &stTextLang,
		"--no-txt-hdr",		"bool", &sTextNoHdr,
		"--channel",		"int", &sSndChannel
	);
	
	for(i=2; i<app_argc; i++) {
		if(!app_argv[i]) continue;
		if(!sRcoFile)
			sRcoFile = app_argv[i];
		else if(!sResLabel)
			sResLabel = app_argv[i];
		else if(!sOutFile)
			sOutFile = app_argv[i];
		else MAIN_INV_CMD_SYNTAX
	}
	if(!sRcoFile || !sResLabel) MAIN_INV_CMD_SYNTAX
	
	if(!sOutFile) sOutFile = sResLabel;
	
	rRCOFile* rco = read_rco(sRcoFile);
	if(!rco) {
		return RETERR_READRCO;
	}
	
	rRCOEntry* entry = find_entry_from_label(&rco->tblMain, sResLabel);
	if(entry) {
		/* if(entry->id == RCO_TABLE_SOUND && entry->type == 1) {
			// handle sound channel stuff
			
		} */
		if(stTextLang) warning("Resource '%s' is not text.", sResLabel);
		if(!dump_resource(sOutFile, entry, dump_output_data, NULL)) {
			error("Unable to dump resource '%s'.", sResLabel);
			return RETERR_GENERIC_FAILED;
		}
		return 0;
	} else if(rco->tblText) {
		// try text
		if(!rcoxml_text_to_int(stTextLang, RCOXML_TABLE_TEXT_LANG, &sTextLang)) {
			if(!sscanf(stTextLang, "%i", &sTextLang))
				warning("Unknown language '%s', defaulting to English.", stTextLang);
		}
		rRCOEntry* rcoNode;
		for(rcoNode = rco->tblText->firstChild; rcoNode; rcoNode = rcoNode->next)
			if(((rRCOTextEntry*)rcoNode->extra)->lang == sTextLang)
				break;
		
		if(!rcoNode) {
			error("Unable to find specified language '%s'.", stTextLang);
			return RETERR_GENERIC_FAILED;
		}
		
		rRCOTextEntry* rte = (rRCOTextEntry*)rcoNode->extra;
		
		int textIdx = find_text_from_label(rco->labels, rte, sResLabel);
		if(textIdx != -1) {
			// dump it
			FILE* fp = openwrite(sOutFile);
			if(fp) {
				if(!sTextNoHdr) {
					if(rte->format == RCO_TEXT_FMT_UTF32) {
						uint32 bom = UTF32_BOM;
						if(rco->eSwap) bom = ENDIAN_SWAP(bom);
						filewrite(fp, &bom, sizeof(bom));
					} else if(rte->format == RCO_TEXT_FMT_UTF8) {
						uint32 bom = UTF8_BOM;
						filewrite(fp, &bom, 3);
					} else {
						uint16 bom = UTF16_BOM;
						if(rco->eSwap) bom = ENDIAN_SWAP(bom);
						filewrite(fp, &bom, sizeof(bom));
					}
				}
				filewrite(fp, rco->labels + rte->indexes[textIdx].labelOffset, rte->indexes[textIdx].length);
				fclose(fp);
				return 0;
			} else {
				error("Unable to open file '%s'.", sOutFile);
				return RETERR_GENERIC_FAILED;
			}
		}
	}
	
	// not found
	error("Unable to find resource named '%s'.", sResLabel);
	return RETERR_GENERIC_FAILED;
}


void get_pack_opts(writerco_options* opts) {
	char *packHdr, *packRes, *packCmp;
	char *zlibMethod;
	int zlibLevel, rlzMode;
	
	packHdr = packRes = packCmp = NULL;
	zlibMethod = NULL;
	zlibLevel = rlzMode = -1;
	
	opts->packHeader = opts->packText = RCO_DATA_COMPRESSION_NONE;
	opts->packImg = opts->packModel = -1;
	opts->packImgCompr = -1;
	
	retrieve_from_opts(FALSE, 5,
		"--pack-hdr",	"text", &packHdr,
		"--pack-res",	"text", &packRes,
		"--pack-cmp",	"text", &packCmp,
		"--zlib-method", "text", &zlibMethod,
		"--zlib-level",	"int", &zlibLevel,
		"--rlz-mode",	"int", &rlzMode
	);
	
#ifdef DISABLE_RLZ
	#define READ_PACK_VAL(a, v) { \
		if(!strcasecmp(a, "none")) v = RCO_DATA_COMPRESSION_NONE; \
		else if(!strcasecmp(a, "zlib")) v = RCO_DATA_COMPRESSION_ZLIB; \
		else { \
			error("Unknown compression type '%s'.", a); \
			exit(RETERR_SYNTAX); \
		} \
	}
#else
	#define READ_PACK_VAL(a, v) { \
		if(!strcasecmp(a, "none")) v = RCO_DATA_COMPRESSION_NONE; \
		else if(!strcasecmp(a, "zlib")) v = RCO_DATA_COMPRESSION_ZLIB; \
		else if(!strcasecmp(a, "rlz")) v = RCO_DATA_COMPRESSION_RLZ; \
		else { \
			error("Unknown compression type '%s'.", a); \
			exit(RETERR_SYNTAX); \
		} \
	}
#endif
	if(packHdr) {
		READ_PACK_VAL(packHdr, opts->packHeader);
		opts->packText = opts->packHeader;
	}
	if(packRes) {
		READ_PACK_VAL(packRes, opts->packImg);
		opts->packModel = opts->packImg;
	}
	if(packCmp) {
		READ_PACK_VAL(packCmp, opts->packImgCompr);
	}
	
	opts->zlibMethod = WRITERCO_ZLIB_METHOD_7Z;
	opts->zlibLevel = 3;
	if(zlibMethod) {
		if(!strcasecmp(zlibMethod, "default")) opts->zlibMethod = WRITERCO_ZLIB_METHOD_ZDEFAULT;
		else if(!strcasecmp(zlibMethod, "filtered")) opts->zlibMethod = WRITERCO_ZLIB_METHOD_ZFILTERED;
		else if(!strcasecmp(zlibMethod, "huffman")) opts->zlibMethod = WRITERCO_ZLIB_METHOD_ZHUFFMAN;
		else if(!strcasecmp(zlibMethod, "rle")) opts->zlibMethod = WRITERCO_ZLIB_METHOD_ZRLE;
		else if(!strcasecmp(zlibMethod, "fixed")) opts->zlibMethod = WRITERCO_ZLIB_METHOD_ZFIXED;
		else if(!strcasecmp(zlibMethod, "7z")) opts->zlibMethod = WRITERCO_ZLIB_METHOD_7Z;
		else {
			error("Unknown zlib method '%s'.", zlibMethod);
			exit(RETERR_SYNTAX);
		}
	}
	// stick in default compression level if using zlib
	if(opts->zlibMethod >= WRITERCO_ZLIB_METHOD_ZDEFAULT && opts->zlibMethod <= WRITERCO_ZLIB_METHOD_ZRLE)
		opts->zlibLevel = 9;
	if(zlibLevel != -1) {
		if(opts->zlibMethod >= WRITERCO_ZLIB_METHOD_ZDEFAULT && opts->zlibMethod <= WRITERCO_ZLIB_METHOD_ZRLE) {
			if(zlibLevel < 0 || zlibLevel > 9) {
				error("zlib compression level must be between 0 and 9.");
				exit(RETERR_SYNTAX);
			}
		} else if(opts->zlibMethod == WRITERCO_ZLIB_METHOD_7Z) {
			if(zlibLevel < 1 || zlibLevel > 4) {
				error("zlib-7z compression level must be between 1 and 4.");
				exit(RETERR_SYNTAX);
			}
		}
		
		opts->zlibLevel = zlibLevel;
	}
	
	opts->rlzMode = -1;
	if(rlzMode != -1) {
		if(rlzMode < 0 || rlzMode > 31) {
			error("RLZ compression mode must be between 0 and 31.");
			exit(RETERR_SYNTAX);
		}
		opts->rlzMode = rlzMode;
	}
}

int main_compile(void) {
	if(app_argc < 4) MAIN_INV_CMD_SYNTAX;
	
	writerco_options opts;
	
	char* sXmlFile = NULL;
	char* sRcoFile = NULL;
	
	Bool sNoConvGim = FALSE;
	Bool sNoConvVag = FALSE;
	Bool sNoConvVsmx = FALSE;
	RcoDumpGimconvOpts gimconvOpts;
	
	gimconvOpts.cmd = gimconvOpts.extFlags = NULL;
	
	int i;
	// parse options
	get_pack_opts(&opts);
	retrieve_from_opts(TRUE, 5,
		"--no-convgim",		"bool", &sNoConvGim,
		"--no-convvag",		"bool", &sNoConvVag,
		"--no-encvsmx",		"bool", &sNoConvVsmx,
		"--gimconv-cmd",	"text", &gimconvOpts.cmd,
		"--gimconv-flags",	"text", &gimconvOpts.extFlags
	);
	
	for(i=2; i<app_argc; i++) {
		if(!app_argv[i]) continue;
		if(!sXmlFile)
			sXmlFile = app_argv[i];
		else if(!sRcoFile)
			sRcoFile = app_argv[i];
		else MAIN_INV_CMD_SYNTAX
	}
	
	if(!sRcoFile || !sXmlFile) MAIN_INV_CMD_SYNTAX
	
	rRCOFile* rco = read_xml(sXmlFile);
	if(!rco) {
		error("Could not read in XML file '%s'.", sXmlFile);
		return RETERR_READXML;
	}
	
	// convert to vag/gim/vsmx where necessary
	if(!sNoConvGim) {
		rco_map_func(rco, NULL, (void*)&gimconvOpts, compile_gimconv_map);
	}
	if(!sNoConvVag) {
		rco_map_func(rco, NULL, NULL, compile_vagconv_map);
	} else
		rco_map_func(rco, NULL, NULL, compile_wavcheck_map); // verify there are no orphaned .wav sound references
	if(!sNoConvVsmx) {
		rco_map_func(rco, NULL, NULL, compile_vsmxconv_map);
	}
	
	if(!write_rco(rco, sRcoFile, opts))
		return RETERR_WRITERCO;
	
	return 0;
}


// only for debugging purposes
int main_rebuild(void) {
	if(app_argc < 4) MAIN_INV_CMD_SYNTAX;
	
	writerco_options opts;
	
	char* sRcoFileIn = NULL;
	char* sRcoFileOut = NULL;
	
	int i;
	// parse options
	get_pack_opts(&opts);
	for(i=2; i<app_argc; i++) {
		if(!app_argv[i]) continue;
		if(!sRcoFileIn)
			sRcoFileIn = app_argv[i];
		else if(!sRcoFileOut)
			sRcoFileOut = app_argv[i];
		else MAIN_INV_CMD_SYNTAX
	}
	
	rRCOFile* rco = read_rco(sRcoFileIn);
	if(!rco) return RETERR_READRCO;

	if(!write_rco(rco, sRcoFileOut, opts))
		return RETERR_WRITERCO;
	
	return 0;
}


int main_vagdec(void) {
	if(app_argc < 4) MAIN_INV_CMD_SYNTAX;
	
	uint channels = app_argc-3;
	int i;
	int* len = (int*)malloc(channels * sizeof(int));
	void** buf = (void**)malloc(channels * sizeof(void*));
	
	channels = 0;
	for(i=2; i<app_argc -1; i++) {
		FILE* fp = openread(app_argv[i]);
		if(fp) {
			const int READ_BUFFER = 256*1024;
			buf[channels] = NULL;
			len[channels] = 0;
			while(!feof(fp)) {
				buf[channels] = realloc(buf[channels], len[channels] + READ_BUFFER);
				len[channels] += fread(((char**)buf)[channels] + len[channels], 1, READ_BUFFER, fp);
			}
			/*
			fseek(fp, 0, SEEK_END);
			len[channels] = ftell(fp);
			rewind(fp);
			
			buf[channels] = malloc(len[channels]);
			fileread(fp, buf[channels], len[channels]);
			*/
			fclose(fp);
			
			if(len[channels])
				channels++;
			else
				warning("No data in file %s.", app_argv[i]);
		} else
			warning("Unable to open file %s", app_argv[i]);
	}
	
	
	Bool success = vag2wav(app_argv[app_argc-1], channels, len, buf);
	free(len);
	while(channels--)
		free(buf[channels]);
	free(buf);
	
	if(success)
		return 0;
	else {
		error("Failed to decode VAG files.");
		return RETERR_GENERIC_FAILED;
	}
}

int main_vagenc(void) {
	if(app_argc < 4) MAIN_INV_CMD_SYNTAX;
	
	uint i, channels; // = app_argc-3;
	uint len;
	void* buf;
	
	channels = wav2vag(app_argv[2], &len, &buf, "");
	if(channels) {
		if(app_argc-3 > 1) {
			if((int)channels > app_argc-3) {
				warning("WAV file contains %d channels, which is more than VAG files specified - will only output the first %d channels.", channels, app_argc-3);
				channels = app_argc-3;
			} else if((int)channels < app_argc-3) {
				warning("WAV file contains %d channels, which is less than VAG files specified.", channels);
			}
		}
		
		// trim .vag extension if autogenerating that
		if(app_argc-3 == 1 && channels > 1) {
			uint nl = strlen(app_argv[3]);
			if(nl > MAX_FILENAME_LEN)
				app_argv[3][MAX_FILENAME_LEN] = '\0';
			else if(!strcasecmp(app_argv[3] + nl - 4, ".vag"))
				app_argv[3][nl-4] = '\0';
		}
		
		Bool writeStdout = !strcmp(app_argv[3], "-");
		for(i=0; i<channels; i++) {
			FILE* fp;
			char fnStatic[MAX_FILENAME_LEN + 12];
			char* fn = fnStatic;
			
			if(app_argc-3 == 1 && !writeStdout) {
				if(channels == 1)
					fn = app_argv[3];
				else
					sprintf(fnStatic, "%s.ch%d.vag", app_argv[3], i);
			} else {
				fn = app_argv[3+i];
			}
			
			if((fp = openwrite(fn))) {
				filewrite(fp, (char*)buf + i*len, len);
				if(strcmp(fn, "-")) fclose(fp); // don't want to fclose stdout right now
			} else
				warning("Failed to write file %s", fn);
		}
		
		return 0;
	} else {
		error("Failed to convert WAV to VAG.");
		return RETERR_GENERIC_FAILED;
	}
}

int main_vsmxdec(void) {
	if(app_argc < 4) MAIN_INV_CMD_SYNTAX;
	
	Bool doDecompile = FALSE;
	FILE *fin = NULL, *fout = NULL;
	
	retrieve_from_opts(TRUE, 1,
		"--decompile",		"bool", &doDecompile
	);
	
	int i;
	for(i=2; i<app_argc; i++) {
		if(app_argv[i]) {
			if(!fin) {
				if(!strcmp(app_argv[i], "-"))
					fin = stdin;
				else {
					if(!(fin = fopen(app_argv[i], "rb"))) {
						error("Cannot open file %s", app_argv[i]);
						return 1;
					}
				}
			} else if(!fout) {
				if(!strcmp(app_argv[i], "-"))
					fout = stdout;
				else {
					if(!(fout = fopen(app_argv[i], "wb"))) {
						error("Cannot open file %s", app_argv[i]);
						return 1;
					}
				}
			} else {
				warning("Don't know what to do with option \"%s\"", app_argv[i]);
			}
		}
	}
	if(!fin || !fout) {
		MAIN_INV_CMD_SYNTAX;
	}
	
	info("Reading VSMX...");
	VsmxMem* vm = readVSMX(fin);
	fclose(fin);
	Bool success = FALSE;
	if(vm) {
		info("Writing decoded output...");
		if(doDecompile)
			success = !VsmxDecompile(vm, fout);
		else
			success = !VsmxDecode(vm, fout);
		freeVsmxMem(vm);
	}
	fclose(fout);
	
	if(success)
		return 0;
	else {
		if(doDecompile) {
			error("Failed to decompile VSMX.");
		} else {
			error("Failed to decode VSMX.");
		}
		return RETERR_GENERIC_FAILED;
	}
}

int main_vsmxenc(void) {
	if(app_argc < 4) MAIN_INV_CMD_SYNTAX;
	
	FILE *fin = NULL, *fout = NULL;
	
	if(!strcmp(app_argv[2], "-"))
		fin = stdin;
	else {
		if(!(fin = fopen(app_argv[2], "rb"))) {
			error("Cannot open file %s", app_argv[2]);
			return 1;
		}
	}
	if(!strcmp(app_argv[3], "-"))
		fout = stdout;
	else {
		if(!(fout = fopen(app_argv[3], "wb"))) {
			error("Cannot open file %s", app_argv[3]);
			return 1;
		}
	}
	
	if(app_argc > 4)
		warning("Extra arguments ignored.");
	
	
	info("Reading and encoding input...");
	VsmxMem* vm = VsmxEncode(fin);
	fclose(fin);
	if(vm) {
		info("Writing VSMX...");
		writeVSMX(fout, vm);
		freeVsmxMem(vm);
		fclose(fout);
		return 0;
	} else {
		error("Failed to encode VSMX.");
		return RETERR_GENERIC_FAILED;
	}
}


void retrieve_from_opts(Bool warnUnk, int num, ...) {
	int i,j;
	va_list ap;
	
	for(i=2; i<app_argc; i++) {
		if(!app_argv[i]) continue;
		if(app_argv[i][0] == app_argv[i][1] && app_argv[i][0] == '-') {
			va_start(ap, num);
			for(j=0; j<num; j++) {
				if(!strcasecmp(app_argv[i], va_arg(ap, const char*))) {
					char* type = va_arg(ap, char*);
					if(!strcmp(type, "bool"))
						*(va_arg(ap, Bool*)) = TRUE;
					else if(i+1 < app_argc) {
						app_argv[i] = NULL;
						char* val = app_argv[++i];
						if(!strcmp(type, "int")) {
							*(va_arg(ap, int*)) = atoi(val);
						} else {
							*(va_arg(ap, char**)) = val;
						}
					} else {
						error("Invalid syntax for option '%s'.", app_argv[i]);
						exit(RETERR_SYNTAX);
					}
					app_argv[i] = NULL;
					break;
				} else {
					// advance the va_arg thingy
					va_arg(ap, void*);
					va_arg(ap, void*);
				}
			}
			va_end(ap);
			if(warnUnk && j==num) {
				error("Unknown option '%s'.", app_argv[i]);
				exit(RETERR_SYNTAX);
				/* warning("Unknown option '%s'.", argv[i]);
				argv[i][0] = '\0'; // don't warn them again */
			}
		}
	}
}
