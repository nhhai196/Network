CC=gcc
CFLAGS=-I -lthread.
DEPS = header.h
OBJ = audiolisten.o audiostreamd.o audiolisten2.o audiostream2d.o
TARGET = audiolisten audiostreamd audiolisten2 audiostream2d

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) 

all: $(TARGET)

audiolisten: $(OBJ)
	$(CC) -o audiolisten audiolisten.o $(CFLAGS)

audiostreamd: $(OBJ)
	$(CC) -o audiostreamd audiostreamd.o $(CFLAGS)

audiolisten2: $(OBJ)
	$(CC) -o audiolisten2 audiolisten2.o $(CFLAGS)

audiostream2d: $(OBJ)
	$(CC) -o audiostream2d audiostream2d.o $(CFLAGS)
	
clean: 
	rm -f *.o $(TARGET) $(OBJ)
