#pragma once

#include <array>
#include <cstdlib>
#include <vector>

namespace lama {

#define STACK_SIZE (1 << 20)

class Stack {
public:
  Stack();

  void beginFunction(size_t nlocals);
  void pushOperand(int32_t value);
  int32_t peakOperand() const;
  int32_t popOperand();

private:
  void checkNonEmptyOperandStack() const;

private:
  std::array<int32_t, STACK_SIZE> data;
  int32_t *base;
  int32_t *top;

  size_t operandStackSize = 0;

  std::vector<int32_t *> baseStack;
};

} // namespace lama
