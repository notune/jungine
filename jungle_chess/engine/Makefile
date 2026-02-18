CXX = g++
CXXFLAGS = -std=c++17 -O3 -march=native -flto -Wall -Wextra -DNDEBUG
LDFLAGS = -flto

TARGET = jungle_engine
SOURCES = main.cpp
HEADERS = types.h board.h movegen.h evaluate.h search.h tt.h

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
	@echo "Build complete: $(TARGET)"
	@strip $(TARGET)
	@ls -lh $(TARGET)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

debug:
	$(CXX) -std=c++17 -g -O0 -Wall -Wextra -o $(TARGET)_debug $(SOURCES)

test: $(TARGET)
	@echo "Testing engine..."
	@echo -e "uci\nisready\nquit" | ./$(TARGET)
	@echo "Basic test passed!"
