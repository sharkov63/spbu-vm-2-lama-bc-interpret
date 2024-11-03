#include "Stack.h"
#include <cstring>
#include <stdexcept>

using namespace lama;

//===----------------------------------------------------------------------===//
// Stack
//===----------------------------------------------------------------------===//

Stack::Stack() {
  top = data.end();
  // __gc_stack_bottom = (size_t)top;
  // __gc_stack_top = __gc_stack_bottom;
}

void Stack::beginFunction(size_t nlocals) {
  if (operandStackSize != 0) {
    throw std::runtime_error("non-empty operand stack upon begging a function");
  }
  baseStack.push_back(base);
  base = top;
  top -= nlocals;
  // Fill with boxed zeros so that GC will skip these
  memset(top, 1, base - top);
}

void Stack::pushOperand(int32_t value) {
  ++operandStackSize;
  --top;
  *top = value;
}

int32_t Stack::peakOperand() const {
  checkNonEmptyOperandStack();
  return *top;
}

int32_t Stack::popOperand() {
  checkNonEmptyOperandStack();
  --operandStackSize;
  return *(top++);
}

void Stack::checkNonEmptyOperandStack() const {
  if (operandStackSize == 0) {
    throw std::runtime_error("cannot pop from empty operand stack");
  }
}
