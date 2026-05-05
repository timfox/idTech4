CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
UNAME_S := $(shell uname -s 2>/dev/null)
ifeq ($(UNAME_S),Linux)
  LDFLAGS += -ldl
endif
TARGET := vulkan_caps

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
