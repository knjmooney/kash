CC        = gcc
PROJ      = kash
OBJECTS   = $(PROJ).o stack.o
CFLAGS    = -Wall -lm -g -O2 -lreadline
LDLIBS    = -lreadline

$(PROJ): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDLIBS)

test: $(PROJ)
	 ./$(PROJ)

.PHONY: clean		
clean:
	rm -f $(PROJ) $(OBJECTS)
