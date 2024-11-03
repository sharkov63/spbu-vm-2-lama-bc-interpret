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
  void pushOperand(Value operand);
  Value peakOperand() const;
  Value popOperand();
  void pushIntOperand(int32_t operand);
  int32_t popIntOperand();

private:
  void checkNonEmptyOperandStack() const;

private:
  std::array<Value, STACK_SIZE> data;
  Value *base;
  Value *top;

  size_t operandStackSize = 0;

  std::vector<Value *> baseStack;
};

} // namespace lama
