# writer: writer.o
# 	gcc writer.o -o writer

# writer.o: writer.c
# 	gcc -c writer.c -o writer.o

CC = gcc
OBJFILES = writer.o
MAIN = writer

all: $(MAIN)

$(MAIN): $(OBJFILES)
	$(CC) -o $(MAIN) $(OBJFILES)

%.o: %.c 
	$(CC) -c -o $@ $<

clean:
	rm -f $(OBJFILES) $(MAIN)