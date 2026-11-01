# Scarfnet pattern simulator — no external dependencies.
#
# Usage:
#   make        — build
#   make run    — build and run
#   make clean  — remove build artifacts

CXX      := clang++
CXXFLAGS := -std=c++17 -O2 \
             -Wno-deprecated-copy \
             -Wno-unused-variable \
             -Wno-unused-parameter \
             -I../../include \
             -MMD -MP

SRC_ROOT := ../../src

SRCS := main.cpp \
        $(SRC_ROOT)/PatternManager.cpp \
        $(SRC_ROOT)/palettes.cpp \
        $(SRC_ROOT)/patterns.cpp \
        $(SRC_ROOT)/log.cpp \
        $(wildcard $(SRC_ROOT)/patterns/*.cpp)

BUILD := build
OBJS  := $(patsubst %.cpp,$(BUILD)/%.o,$(notdir $(SRCS)))
vpath %.cpp . $(SRC_ROOT) $(SRC_ROOT)/patterns

TARGET := scarfnet-sim

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: %.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $@

run: all
	./$(TARGET)

clean:
	rm -rf $(TARGET) $(BUILD)

-include $(OBJS:.o=.d)
