CXX = gcc
TARGET = hello
SRC = $(wildcard *.c)
OBJ = $(patsubst %.c, %.o, $(SRC))

CXXFLAGS = -c -Wall

$(TARGET): $(OBJ)
	$(CXX) -o $@ $^

%.o: %.c
	$(CXX) $(CXXFLAGS) $< -o $@

.PHONY:clean
clean:
	rm -f *.o
