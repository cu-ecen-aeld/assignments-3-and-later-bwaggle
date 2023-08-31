# writer: writer.o
# 	gcc writer.o -o writer

# writer.o: writer.c
# 	gcc -c writer.c -o writer.o

CC ?= $(CROSS_COMPILE)gcc
OBJFILES ?= aesdsocket.o
MAIN ?= aesdsocket
CFLAGS ?= -g -Wall -Werror -pthread

all: $(MAIN)

$(MAIN): $(OBJFILES)
	$(CC) -o $(MAIN) $(OBJFILES) ${CFLAGS}

%.o: %.c 
	$(CC) -c -o $@ $<

clean:
	rm -f $(OBJFILES) $(MAIN)