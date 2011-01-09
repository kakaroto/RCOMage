
CC = gcc
CFLAGS = -Wall  `pkg-config --cflags libxml-2.0` `pkg-config --cflags zlib`
LIBS = -lm -lstdc++ `pkg-config --libs libxml-2.0` `pkg-config --libs zlib`
# replace -liconv with -lglib on linux
TARGET = rcomage
#SEVENZSRC = 7z/7zdeflate.cc 7z/7zlzma.cc 7z/AriBitCoder.cc 7z/CRC.cc 7z/DeflateDecoder.cc 7z/DeflateEncoder.cc 7z/HuffmanEncoder.cc 7z/IInOutStreams.cc 7z/InByte.cc 7z/LenCoder.cc 7z/LiteralCoder.cc 7z/LSBFDecoder.cc 7z/LSBFEncoder.cc 7z/LZMA.cc 7z/LZMADecoder.cc 7z/LZMAEncoder.cc 7z/OutByte.cc 7z/WindowIn.cc 7z/WindowOut.cc
SEVENZSRC = 7z/7zdeflate.cc 7z/CRC.cc 7z/DeflateEncoder.cc 7z/HuffmanEncoder.cc 7z/IInOutStreams.cc 7z/LSBFEncoder.cc 7z/OutByte.cc 7z/WindowIn.cc
SRC = general.c globdefs.c main.c rcodump.c rcomain.c rcoreader.c rcowriter.c rlzpack.c vaghandler.c xmlread.c xmlwrite.c vsmx.c configscan.c
OBJ = $(SRC:.c=.o)
SEVENZOBJ = $(SEVENZSRC:.cc=.o)

ifeq ($(DEBUG), 1)
CFLAGS += -g
LIBS += -g
else
CFLAGS += -O2
endif

COMPILE = $(CC) $(CFLAGS)

all: $(TARGET)
$(TARGET): $(OBJ) $(SEVENZOBJ)
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