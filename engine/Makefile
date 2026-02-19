CXX=g++
CXXFLAGS=-O3 -march=native -std=c++17 -Wall -Wextra

SRCS=jungle.cpp search.cpp main.cpp
OBJS=$(SRCS:.cpp=.o)
TARGET=jungleai

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
