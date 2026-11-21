CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 \
         -Iinclude -Isrc \
         $(shell pkg-config --cflags raylib)

LDFLAGS = $(shell pkg-config --libs raylib) -lm

TARGET = ev_planner

SRC = $(shell find src -name '*.c' ! -path 'src/tests/*')
HDR = $(shell find src -name '*.h')

# Everything except the app entry point and the GUI, for linking tests.
LIB_SRC = $(filter-out src/main.c src/gui/%,$(SRC))

TESTS = $(patsubst tests/%.c,%,$(wildcard tests/test_*.c))

.PHONY: all clean run test baseline

all: $(TARGET)

$(TARGET): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

test_%: tests/test_%.c $(LIB_SRC) $(HDR)
	$(CC) $(CFLAGS) $< $(LIB_SRC) -o $@ $(LDFLAGS)

# Builds and runs every tests/test_*.c; shows a test's output only if it fails.
test: $(TESTS)
	@status=0; \
	for t in $(TESTS); do \
		if ./$$t > .test_output 2>&1; then \
			echo "PASS  $$t"; \
		else \
			echo "FAIL  $$t"; cat .test_output; status=1; \
		fi; \
	done; \
	rm -f .test_output; \
	exit $$status

# Runs fixed routes headlessly and diffs them against the recorded baseline.
baseline_routes: tools/baseline_routes.c $(SRC) $(HDR)
	$(CC) $(CFLAGS) -Dmain=app_main $< $(filter-out src/main.c,$(SRC)) \
	      -o $@ $(LDFLAGS)

baseline: baseline_routes
	@./baseline_routes | grep '^@@' > .baseline_output
	@if grep -v '^#' docs/baseline_routes.txt | diff - .baseline_output; \
	then echo "Baseline routes: identical"; rm -f .baseline_output; \
	else echo "Baseline routes: DIFFERENT"; rm -f .baseline_output; exit 1; fi

clean:
	rm -f $(TARGET) *.o $(TESTS) .test_output baseline_routes .baseline_output

run: $(TARGET)
	./$(TARGET)
