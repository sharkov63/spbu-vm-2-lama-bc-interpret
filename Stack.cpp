#include "Stack.h"
#include <cstring>
#include <stdexcept>

using namespace lama;

//===----------------------------------------------------------------------===//
// Stack
//===----------------------------------------------------------------------===//

Stack::Stack() {
  frame.top = frame.base = data.end();
  // __gc_stack_bottom = (size_t)top;
  // __gc_stack_top = __gc_stack_bottom;
}

void Stack::beginFunction(size_t nlocals) {
  if (frame.operandStackSize != 0) {
    throw std::runtime_error(
        "non-empty operand stack upon beginning a function");
  }
  frameStack.push_back(frame);
  frame.base = frame.top;
  frame.top = frame.base - nlocals;
  frame.nlocals = nlocals;
  frame.operandStackSize = 0;
  // Fill with boxed zeros so that GC will skip these
  memset(frame.top, 1, frame.base - frame.top);
}

void Stack::endFunction() {
  if (isEmpty()) {
    throw std::runtime_error("no function to end");
  }
  frame = frameStack.back();
  frameStack.pop_back();
}

void Stack::pushOperand(Value value) {
  ++frame.operandStackSize;
  --frame.top;
  *frame.top = value;
}

Value Stack::peakOperand() const {
  checkNonEmptyOperandStack();
  return *frame.top;
}

Value Stack::popOperand() {
  checkNonEmptyOperandStack();
  --frame.operandStackSize;
  return *(frame.top++);
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

Value &Stack::accessLocal(int32_t index) {
  if (index < 0 || index >= frame.nlocals) {
    throw std::runtime_error(fmt::format(
        "access local variable out of bounds: index {} is not in [0, {})",
        index, frame.nlocals));
  }
  return frame.base[-index];
}

void Stack::checkNonEmptyOperandStack() const {
  if (frame.operandStackSize == 0) {
    throw std::runtime_error("cannot pop from empty operand stack");
  }
}
