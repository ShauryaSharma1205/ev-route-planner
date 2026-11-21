CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 \
         -Iinclude -Isrc \
         $(shell pkg-config --cflags raylib)

LDFLAGS = $(shell pkg-config --libs raylib) -lm

TARGET = ev_planner

SRC = $(shell find src -name '*.c' ! -path 'src/tests/*')

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o \
	      test_dijkstra test_battery \
	      test_stations test_battery_routing \
	      test_road_speed

run: $(TARGET)
	./$(TARGET)