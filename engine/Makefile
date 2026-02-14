CXX = g++
CXXFLAGS = -std=c++17 -O3 -march=native -flto -DNDEBUG -Wall -Wextra -Wno-unused-parameter
LDFLAGS = -lpthread -flto
TARGET = jungle

SRCS = main.cpp board.cpp search.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Dependencies
main.o: main.cpp search.h board.h types.h
board.o: board.cpp board.h types.h
search.o: search.cpp search.h board.h types.h

debug: CXXFLAGS = -std=c++17 -g -O0 -Wall -Wextra -fsanitize=address,undefined
debug: LDFLAGS = -fsanitize=address,undefined -lpthread
debug: clean $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean debug
