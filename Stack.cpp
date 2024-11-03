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
    throw std::runtime_error(
        "non-empty operand stack upon beginning a function");
  }
  baseStack.push_back(base);
  base = top;
  top -= nlocals;
  // Fill with boxed zeros so that GC will skip these
  memset(top, 1, base - top);
}

void Stack::endFunction() {
  if (baseStack.empty()) {
    throw std::runtime_error("empty base stack at function end");
  }
  top = base;
  base = baseStack.back();
  baseStack.pop_back();
  operandStackSize = 0;
}

void Stack::pushOperand(Value value) {
  ++operandStackSize;
  --top;
  *top = value;
}

Value Stack::peakOperand() const {
  checkNonEmptyOperandStack();
  return *top;
}

Value Stack::popOperand() {
  checkNonEmptyOperandStack();
  --operandStackSize;
  return *(top++);
}

void Stack::pushIntOperand(int32_t operand) { pushOperand(boxInt(operand)); }

int32_t Stack::popIntOperand() {
  Value operand = popOperand();
  if (!valueIsInt(operand)) {
    throw std::runtime_error(fmt::format(
        "expected a (boxed) number at the operand stack top, found {:#x}",
        operand));
  }
  return unboxInt(operand);
}

void Stack::checkNonEmptyOperandStack() const {
  if (operandStackSize == 0) {
    throw std::runtime_error("cannot pop from empty operand stack");
  }
}
