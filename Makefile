CC=gcc
CXX=g++
COMMON_FLAGS=-m32 -g2 -fstack-protector-all
INTERPRETER_FLAGS=$(COMMON_FLAGS) -DFMT_HEADER_ONLY -ILama

all: runtime YAILama

runtime:
	$(MAKE) -C Lama/runtime

Main.o: Main.cpp ByteFile.h Interpreter.h
	$(CXX) -o $@ $(INTERPRETER_FLAGS) -c Main.cpp

ByteFile.o: ByteFile.cpp ByteFile.h
	$(CXX) -o $@ $(INTERPRETER_FLAGS) -c ByteFile.cpp

Interpreter.o: Interpreter.cpp Interpreter.h
	$(CXX) -o $@ $(INTERPRETER_FLAGS) -c Interpreter.cpp

YAILama: Main.o ByteFile.o Interpreter.o
	$(CXX) -o $@ $(INTERPRETER_FLAGS) Lama/runtime/runtime.o Lama/runtime/gc.o $^

clean:
	$(RM) *.a *.o *~ YAILama

.PHONY: all clean runtime
