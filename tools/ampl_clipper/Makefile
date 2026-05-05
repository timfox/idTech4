# AMPL/MathProg + Clipper2 demo tool (see README.md)

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

CLIPPER_CPPFLAGS = -I../../third_party/clipper2/CPP/Clipper2Lib/include
CLIPPER_SRCS = \
	../../third_party/clipper2/CPP/Clipper2Lib/src/clipper.engine.cpp \
	../../third_party/clipper2/CPP/Clipper2Lib/src/clipper.offset.cpp \
	../../third_party/clipper2/CPP/Clipper2Lib/src/clipper.rectclip.cpp \
	../../third_party/clipper2/CPP/Clipper2Lib/src/clipper.triangulation.cpp

OBJS = main.o clipper.engine.o clipper.offset.o clipper.rectclip.o clipper.triangulation.o

.PHONY: all clean

all: ampl_clipper

ampl_clipper: $(OBJS)
	$(CXX) $(CXXFLAGS) -o ampl_clipper $(OBJS)

main.o: main.cpp
	$(CXX) $(CXXFLAGS) $(CLIPPER_CPPFLAGS) -c main.cpp -o main.o

clipper.engine.o: ../../third_party/clipper2/CPP/Clipper2Lib/src/clipper.engine.cpp
	$(CXX) $(CXXFLAGS) $(CLIPPER_CPPFLAGS) -c $< -o $@

clipper.offset.o: ../../third_party/clipper2/CPP/Clipper2Lib/src/clipper.offset.cpp
	$(CXX) $(CXXFLAGS) $(CLIPPER_CPPFLAGS) -c $< -o $@

clipper.rectclip.o: ../../third_party/clipper2/CPP/Clipper2Lib/src/clipper.rectclip.cpp
	$(CXX) $(CXXFLAGS) $(CLIPPER_CPPFLAGS) -c $< -o $@

clipper.triangulation.o: ../../third_party/clipper2/CPP/Clipper2Lib/src/clipper.triangulation.cpp
	$(CXX) $(CXXFLAGS) $(CLIPPER_CPPFLAGS) -c $< -o $@

clean:
	rm -f ampl_clipper main.o clipper.engine.o clipper.offset.o clipper.rectclip.o clipper.triangulation.o
