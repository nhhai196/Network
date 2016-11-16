CC=gcc
CFLAGS=-I -lthread.
DEPS = header.h
OBJ = audiolisten.o audiostreamd.o
TARGET = audiolisten audiostreamd

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) 

all: $(TARGET)

audiolisten: $(OBJ)
	$(CC) -o audiolisten audiolisten.o $(CFLAGS)

audiostreamd: $(OBJ)
	$(CC) -o audiostreamd audiostreamd.o $(CFLAGS)
	
clean: 
	rm -f *.o $(TARGET) $(OBJ)
