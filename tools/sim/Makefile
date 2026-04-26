# Scarfnet pattern simulator — no external dependencies.
#
# Usage:
#   make        — build
#   make run    — build and run
#   make clean  — remove build artifact

CXX      := clang++
CXXFLAGS := -std=c++17 -O2 \
             -Wno-deprecated-copy \
             -Wno-unused-variable \
             -Wno-unused-parameter \
             -I../../include

SRC_ROOT := ../../src

SRCS := main.cpp \
        $(SRC_ROOT)/PatternManager.cpp \
        $(SRC_ROOT)/palettes.cpp \
        $(SRC_ROOT)/patterns.cpp \
        $(SRC_ROOT)/log.cpp \
        $(SRC_ROOT)/patterns/breathe.cpp \
        $(SRC_ROOT)/patterns/colorwaves.cpp \
        $(SRC_ROOT)/patterns/confetti.cpp \
        $(SRC_ROOT)/patterns/cylon.cpp \
        $(SRC_ROOT)/patterns/dance.cpp \
        $(SRC_ROOT)/patterns/firework.cpp \
        $(SRC_ROOT)/patterns/fractal.cpp \
        $(SRC_ROOT)/patterns/generative.cpp \
        $(SRC_ROOT)/patterns/pride.cpp \
        $(SRC_ROOT)/patterns/sparkle.cpp \
        $(SRC_ROOT)/patterns/digital.cpp \
        $(SRC_ROOT)/patterns/debug.cpp \
        $(SRC_ROOT)/patterns/beat.cpp

TARGET := scarfnet-sim

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)
