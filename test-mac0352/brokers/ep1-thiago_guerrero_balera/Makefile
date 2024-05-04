ODIR=src
IDIR=interfaces
CC=gcc
CFLAGS=-I$(IDIR) -lm -pthread

_DEPS = message.h mqtt.h util.h config.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = message.o mqtt.o util.o broker.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

###############################
broker: $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS)

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f broker $(ODIR)/*.o
