CXX = g++

# Toggle LTO at build time:
#   make          -> LTO on (default, best runtime perf, slower link)
#   make LTO=0    -> LTO off (faster link, no lto-wrapper warning)
#   make nolto    -> same as LTO=0 (handy shorthand)
LTO ?= 1
ifeq ($(LTO),1)
LTO_FLAG = -flto
else
LTO_FLAG =
endif

CXXFLAGS = -std=c++17 -O3 $(LTO_FLAG) -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
LDFLAGS = -O3 $(LTO_FLAG) -s

SRCS = src/main.cpp src/gc.cpp src/value.cpp src/env.cpp src/lexer.cpp src/parser.cpp src/builtins.cpp src/interp.cpp src/bytecode.cpp src/compiler.cpp src/vm.cpp src/context.cpp
OBJS = $(SRCS:src/%.cpp=%.o)

all: njs

njs: $(OBJS)
	$(CXX) $(LDFLAGS) -o njs $(OBJS)

%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

main.o: src/main.cpp src/nyblejs.h src/value.h src/lexer.h src/ast.h src/parser.h src/env.h src/interp.h src/builtins.h src/gc.h src/bytecode.h src/compiler.h src/vm.h src/cats.h
gc.o: src/gc.cpp src/gc.h src/value.h
value.o: src/value.cpp src/value.h src/gc.h src/env.h
env.o: src/env.cpp src/env.h src/value.h src/gc.h
lexer.o: src/lexer.cpp src/lexer.h
parser.o: src/parser.cpp src/parser.h src/ast.h src/value.h
builtins.o: src/builtins.cpp src/builtins.h src/env.h src/value.h src/gc.h
interp.o: src/interp.cpp src/interp.h src/ast.h src/env.h src/builtins.h src/value.h src/gc.h
bytecode.o: src/bytecode.cpp src/bytecode.h
compiler.o: src/compiler.cpp src/compiler.h src/bytecode.h src/ast.h src/value.h
vm.o: src/vm.cpp src/vm.h src/compiler.h src/env.h src/gc.h src/builtins.h src/interp.h src/bytecode.h src/ast.h src/value.h
context.o: src/context.cpp src/context.h src/vm.h src/interp.h src/lexer.h src/parser.h src/compiler.h src/builtins.h src/env.h src/gc.h src/ast.h src/value.h

clean:
	del /Q njs.exe $(OBJS) 2>nul || rm -f njs $(OBJS)

run: njs
	./njs

# Rebuild cleanly without LTO
nolto:
	$(MAKE) clean
	$(MAKE) LTO=0 all

.PHONY: all clean run nolto
