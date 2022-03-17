LDLIBS=-lm -lstdc++

PROGRAMS=main

all: $(PROGRAMS) obj_clean

clean: obj_clean
	rm -f *~ $(PROGRAMS)

obj_clean:
	rm -f *.o

main: main.o lcgrand.o

# DO NOT DELETE
%.o: %.c
	$(CC) -c -o $@ $<
