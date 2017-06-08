################################################
#                                              #
#              Playerise Makefile              #
#                                              #
################################################

ifeq ($(OS),Windows_NT)
	SHELL=C:/Windows/System32/cmd.exe
endif

ifndef CXX
	CXX = g++
endif

CXXFLAGS = -w -O2

ifneq ($(OS),Windows_NT)
	LDFLAGS += -lncurses
endif

SRC = Playerise.cpp

OBJS = Playerise.o

EXC = Playerise

.PHONY:
all : $(EXC)

$(EXC) : $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(OBJS) : $(SRC)
	$(CXX) $(CXXFLAGS) -c $^

.PHONY:
clean:
	rm -rf *.o Playerise