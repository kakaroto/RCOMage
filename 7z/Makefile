
CC = gcc
SRC = 7zdeflate.cc CRC.cc DeflateEncoder.cc HuffmanEncoder.cc IInOutStreams.cc LSBFEncoder.cc OutByte.cc WindowIn.cc
OBJ = $(SRC:.cc=.o)

all: $(OBJ)
%.o:%.cc
	$(CC) -O3 -D__GNU_C__ -c $<
