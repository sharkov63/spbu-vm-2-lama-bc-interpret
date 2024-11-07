CC=gcc
CXX=g++
COMMON_FLAGS=-m32 -g2 -fstack-protector-all -O2
INTERPRETER_FLAGS=$(COMMON_FLAGS) -Ifmt/include -DFMT_HEADER_ONLY 
#REGRESSION_TESTS=$(sort $(filter-out test111, $(notdir $(basename $(wildcard Lama/regression/test*.lama)))))
LAMAC=lamac
YAILAMA=$(realpath ./YAILama)

all: YAILama

runtime:
	$(MAKE) -C runtime

Main.o: Main.cpp ByteFile.h Interpreter.h
	$(CXX) -o $@ $(INTERPRETER_FLAGS) -c Main.cpp

GlobalArea.o: GlobalArea.s
	$(CXX) -o $@ $(INTERPRETER_FLAGS) -c GlobalArea.s

ByteFile.o: ByteFile.cpp ByteFile.h
	$(CXX) -o $@ $(INTERPRETER_FLAGS) -c ByteFile.cpp

Interpreter.o: Interpreter.cpp Interpreter.h
	$(CXX) -o $@ $(INTERPRETER_FLAGS) -c Interpreter.cpp

Barray_.o: Barray_.s
	$(CC) -o $@ $(INTERPRETER_FLAGS) -c Barray_.s

Bsexp_.o: Bsexp_.s
	$(CC) -o $@ $(INTERPRETER_FLAGS) -c Bsexp_.s

Bclosure_.o: Bclosure_.s
	$(CC) -o $@ $(INTERPRETER_FLAGS) -c Bclosure_.s

YAILama: Main.o GlobalArea.o ByteFile.o Interpreter.o Barray_.o Bsexp_.o Bclosure_.o runtime
	$(CXX) -o $@ $(INTERPRETER_FLAGS) runtime/runtime.o runtime/gc.o Main.o GlobalArea.o ByteFile.o Interpreter.o Barray_.o Bsexp_.o Bclosure_.o

clean:
	$(RM) *.a *.o *~ YAILama
	$(MAKE) clean -C runtime
	$(MAKE) clean -C regression
	$(MAKE) clean -C performance

regression: YAILama
	$(MAKE) clean check -j8 -C regression

regression-expressions: YAILama
	$(MAKE) clean check -j8 -C regression/expressions
	$(MAKE) clean check -j8 -C regression/deep-expressions

performance: YAILama
	$(MAKE) clean check -C performance

.PHONY: all clean runtime regression regression-expressions performance
