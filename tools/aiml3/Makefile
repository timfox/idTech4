# AIML 3.0 Core reference tool — POSIX (Linux/macOS). Windows: compile sources with MSVC similarly to tools/vulkan_caps.

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -I.

SRCS := main.cpp normalize.cpp match.cpp template.cpp interpreter.cpp load_json.cpp load_xml.cpp
OBJS := $(SRCS:.cpp=.o)
TARGET := aiml3

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp aiml3_internal.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
