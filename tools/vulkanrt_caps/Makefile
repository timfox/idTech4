# Linux / macOS (clang): make
# Windows: use MSVC cl.exe or add a CMake target; this Makefile is POSIX-oriented.

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

UNAME_S := $(shell uname -s 2>/dev/null)
ifeq ($(UNAME_S),Linux)
  LDFLAGS += -ldl
endif
ifeq ($(UNAME_S),Darwin)
  # MoltenVK often exposes libvulkan.dylib; loader may be in SDK
  LDFLAGS +=
endif

TARGET := vulkanrt_caps

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
