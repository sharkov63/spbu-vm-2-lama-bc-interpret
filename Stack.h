#pragma once

#include "Value.h"
#include <array>
#include <cstdlib>
#include <vector>

namespace lama {

#define STACK_SIZE (1 << 20)

class Stack {
public:
  Stack();

  void beginFunction(size_t nlocals);
  void endFunction();

  void pushOperand(Value operand);
  Value peakOperand() const;
  Value popOperand();
  void pushIntOperand(int32_t operand);
  int32_t popIntOperand();

  bool isEmpty() const { return frameStack.empty(); }
  bool isNotEmpty() const { return !isEmpty(); }

  Value &accessLocal(int32_t index);

private:
  void checkNonEmptyOperandStack() const;

  struct Frame {
    Value *base;
    Value *top;
    size_t nlocals;
    size_t operandStackSize = 0;
  };

private:
  std::array<Value, STACK_SIZE> data;
  Frame frame;
  std::vector<Frame> frameStack;
};

} // namespace lama
