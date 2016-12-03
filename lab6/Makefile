CC=gcc
CFLAGS=-I.
DEPS = header.h
OBJ = overlayrouter.o overlaybuild.o
TARGET = overlayrouter overlaybuild

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) 

all: $(TARGET)

overlayrouter: $(OBJ)
	$(CC) -o  overlayrouter overlayrouter.o $(CFLAGS)

overlaybuild: $(OBJ)
	$(CC) -o overlaybuild overlaybuild.o $(CFLAGS)

traffic_snd: $(OBJ)
	$(CC) -o traffic_snd traffic_snd.o $(CFLAGS)

traffic_rcv: $(OBJ)
	$(CC) -o traffic_rcv traffic_rcv.o $(CFLAGS)
	
myping: $(OBJ)
	$(CC) -o myping myping.o $(CFLAGS)

mypingd: $(OBJ)
	$(CC) -o mypingd mypingd.o $(CFLAGS)
	
wetalk: $(OBJ)
	$(CC) -o wetalk wetalk.o $(CFLAGS)	
	
clean: 
	rm -f *.o $(TARGET) $(OBJ)