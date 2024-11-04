CC=gcc
CXX=g++
COMMON_FLAGS=-m32 -g2 -fstack-protector-all
INTERPRETER_FLAGS=$(COMMON_FLAGS) -DFMT_HEADER_ONLY -ILama
REGRESSION_TESTS=$(sort $(filter-out test111, $(notdir $(basename $(wildcard Lama/regression/test*.lama)))))
LAMAC=lamac
YAILAMA=$(realpath ./YAILama)

all: runtime YAILama

runtime:
	$(MAKE) -C Lama/runtime

Main.o: Main.cpp ByteFile.h Interpreter.h
	$(CXX) -o $@ $(INTERPRETER_FLAGS) -c Main.cpp

GlobalArea.o: GlobalArea.cpp GlobalArea.h
	$(CXX) -o $@ $(INTERPRETER_FLAGS) -c GlobalArea.cpp

ByteFile.o: ByteFile.cpp ByteFile.h
	$(CXX) -o $@ $(INTERPRETER_FLAGS) -c ByteFile.cpp

Stack.o: Stack.cpp Stack.h
	$(CXX) -o $@ $(INTERPRETER_FLAGS) -c Stack.cpp

Interpreter.o: Interpreter.cpp Interpreter.h
	$(CXX) -o $@ $(INTERPRETER_FLAGS) -c Interpreter.cpp

Barray_.o: Barray_.s
	$(CC) -o $@ $(INTERPRETER_FLAGS) -c Barray_.s

Bsexp_.o: Bsexp_.s
	$(CC) -o $@ $(INTERPRETER_FLAGS) -c Bsexp_.s

Bclosure_.o: Bclosure_.s
	$(CC) -o $@ $(INTERPRETER_FLAGS) -c Bclosure_.s

YAILama: Main.o GlobalArea.o ByteFile.o Stack.o Interpreter.o Barray_.o Bsexp_.o Bclosure_.o
	$(CXX) -o $@ $(INTERPRETER_FLAGS) Lama/runtime/runtime.o Lama/runtime/gc.o $^

clean:
	$(RM) *.a *.o *~ YAILama
	$(MAKE) clean -C Lama/runtime

test_regression: YAILama $(REGRESSION_TESTS)

$(REGRESSION_TESTS): %: 
	@echo "Running regression test $@"
	@cd Lama/regression && \
	$(LAMAC) $@.lama && cat $@.input | ./$@ > $@.lamac.log && \
	$(LAMAC) -b $@.lama && \
	cat $@.input | $(YAILAMA) $@.bc > $@.yailama.log && \
	diff $@.lamac.log $@.yailama.log

.PHONY: all clean runtime test_regression
