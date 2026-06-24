CXX = g++
CXXFLAGS = -std=c++17 -O3 -flto -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
LDFLAGS = -O3 -flto -s

SRC = src/main.cpp
OBJ = main.o

all: nyble

nyble: $(OBJ)
	$(CXX) $(LDFLAGS) -o nyble $(OBJ)

$(OBJ): $(SRC) src/value.h src/lexer.h src/ast.h src/parser.h src/env.h src/interp.h src/builtins.h src/gc.h src/bytecode.h src/compiler.h src/vm.h
	$(CXX) $(CXXFLAGS) -c $(SRC) -o $(OBJ)

clean:
	del /Q nyble.exe $(OBJ) 2>nul || rm -f nyble $(OBJ)

run: nyble
	./nyble

.PHONY: all clean run
