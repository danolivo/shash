OBJS = shash.o

all: $(OBJS)
	gcc $^ -lm -o $@
