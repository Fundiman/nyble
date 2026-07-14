CXX = g++
CXXFLAGS = -std=c++17 -O3 -flto -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
LDFLAGS = -O3 -flto -s

SRC = src/main.cpp
OBJ = main.o

all: njs

njs: $(OBJ)
	$(CXX) $(LDFLAGS) -o njs $(OBJ)

$(OBJ): $(SRC) src/nyblejs.h src/value.h src/lexer.h src/ast.h src/parser.h src/env.h src/interp.h src/builtins.h src/gc.h src/bytecode.h src/compiler.h src/vm.h src/quotes.h
	$(CXX) $(CXXFLAGS) -c $(SRC) -o $(OBJ)

clean:
	del /Q njs.exe $(OBJ) 2>nul || rm -f njs $(OBJ)

run: njs
	./njs

.PHONY: all clean run
