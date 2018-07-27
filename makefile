OBJS = shash.o test.o

all: $(OBJS)
	gcc $^ -lm -I. -o $@
