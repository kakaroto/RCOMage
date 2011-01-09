
CC = gcc
CFLAGS = -Wall -x c++ -D__GNU_C__  `pkg-config --cflags libxml-2.0`
LIBS = -lm -lstdc++ `pkg-config --libs libxml-2.0`
# replace -liconv with -lglib on linux
TARGET = rcomage
#SEVENZSRC = 7z/7zdeflate.cc 7z/7zlzma.cc 7z/AriBitCoder.cc 7z/CRC.cc 7z/DeflateDecoder.cc 7z/DeflateEncoder.cc 7z/HuffmanEncoder.cc 7z/IInOutStreams.cc 7z/InByte.cc 7z/LenCoder.cc 7z/LiteralCoder.cc 7z/LSBFDecoder.cc 7z/LSBFEncoder.cc 7z/LZMA.cc 7z/LZMADecoder.cc 7z/LZMAEncoder.cc 7z/OutByte.cc 7z/WindowIn.cc 7z/WindowOut.cc
SEVENZSRC = 7z/7zdeflate.cc 7z/CRC.cc 7z/DeflateEncoder.cc 7z/HuffmanEncoder.cc 7z/IInOutStreams.cc 7z/LSBFEncoder.cc 7z/OutByte.cc 7z/WindowIn.cc
SRC = general.c globdefs.c main.c rcodump.c rcomain.c rcoreader.c rcowriter.c rlzpack.c vaghandler.c xmlread.c xmlwrite.c vsmx.c configscan.c
OBJ = $(SRC:.c=.o)
SEVENZOBJ = $(SEVENZSRC:.cc=.o)
ZLIBOBJS = zlib/adler32.o zlib/compress.o zlib/crc32.o zlib/gzio.o zlib/uncompr.o zlib/deflate.o zlib/trees.o \
           zlib/zutil.o zlib/inflate.o zlib/infback.o zlib/inftrees.o zlib/inffast.o
#ICONVOBJS = iconv/iconv.o iconv/localcharset.o
ICONVOBJS = 
#LIBXML2OBJS = libxml2/c14n.o libxml2/catalog.o libxml2/chvalid.o libxml2/debugXML.o libxml2/dict.o libxml2/DOCBparser.o libxml2/encoding.o libxml2/entities.o libxml2/error.o libxml2/globals.o libxml2/hash.o libxml2/HTMLparser.o libxml2/HTMLtree.o libxml2/list.o libxml2/parser.o libxml2/parserInternals.o libxml2/pattern.o libxml2/relaxng.o libxml2/SAX.o libxml2/SAX2.o libxml2/schematron.o libxml2/threads.o libxml2/tree.o libxml2/uri.o libxml2/valid.o libxml2/xinclude.o libxml2/xlink.o libxml2/xmlIO.o libxml2/xmlmemory.o libxml2/xmlmodule.o libxml2/xmlregexp.o libxml2/xmlsave.o libxml2/xmlschemas.o libxml2/xmlschemastypes.o libxml2/xmlstring.o libxml2/xmlunicode.o libxml2/xpath.o libxml2/xpointer.o
#libxml2/xmlwriter.o libxml2/xmlreader.o 
#LIBXML2OBJS = libxml2/libxml2.a
LIBXML2OBJS = 

ifeq ($(DEBUG), 1)
CFLAGS += -g
LIBS += -g
else
CFLAGS += -O2
endif

COMPILE = $(CC) -I ./include $(CFLAGS)

all: $(TARGET)
$(TARGET): $(OBJ) $(SEVENZOBJ) $(ZLIBOBJS) $(ICONVOBJS) $(LIBXML2OBJS)
	$(CC) -o $@ $^ $(LIBS)
	strip $@

$(SEVENZOBJ):
	cd 7z && make all
	cd ..
$(ZLIBOBJS):
	cd zlib && make && cd ..

#general.o: general.c
#	$(COMPILE) -lm -c -o $@ $^
#vaghandler.o: vaghandler.c
#	$(COMPILE) -lm -c -o $@ $^
#rcodump.o: rcodump.c
#	$(COMPILE) -liconv -c -o $@ $^
#xmlread.o: xmlread.c
#	$(COMPILE) -lxml2 -liconv -c -o $@ $^
%.o:%.c
	$(COMPILE) -c $<

clean:
	rm -f $(OBJ) $(SEVENZOBJ) $(ZLIBOBJS)