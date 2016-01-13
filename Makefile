#
# http://www.gnu.org/software/make/manual/make.html
#
CPP=g++
INCLUDES=
CFLAGS=-Wall -ggdb -std=c++0x
LDFLAGS=
EXE=example.out detect-au.out detect-wav.out
SRC=DtmfDetector.cpp DtmfGenerator.cpp
OBJ=$(patsubst %.cpp,obj/%.o,$(SRC))

#
# This is here to prevent Make from deleting secondary files.
#
.SECONDARY:
	
debug: CFLAGS += -DDEBUG=1
debug: all

#
# $< is the first dependency in the dependency list
# $@ is the target name
#
all: dirs $(addprefix bin/, $(EXE))


LIB_NAMES = wave
LIBS = $(patsubst %,bin/lib%.a,$(LIB_NAMES))

dirs:
	mkdir -p obj
	mkdir -p bin

bin/%.out: obj/%.o $(OBJ) $(LIB_NAMES)
	$(CPP) $(CFLAGS) $< $(OBJ) $(LDFLAGS) $(LIBS) -o $@

obj/%.o : %.cpp
	$(CPP) $(CFLAGS) $< $(INCLUDES) -c -o $@

$(LIB_NAMES):
	make -C ../$@
	ln -sf ../../$@/DBG/lib$@.a bin


clean:
	rm -f obj/*
	rm -f bin/*
