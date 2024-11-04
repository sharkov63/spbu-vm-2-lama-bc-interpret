#include "Stack.h"
#include "Error.h"
#include <cstring>
#include <iostream>

using namespace lama;

//===----------------------------------------------------------------------===//
// Stack
//===----------------------------------------------------------------------===//

Stack::Stack() {
  frame.base = data.end();
  // Two arguments to main: argc and argv
  frame.top = frame.base - 2;
  frame.operandStackSize = 2;
  // __gc_stack_bottom = (size_t)top;
  // __gc_stack_top = __gc_stack_bottom;
}

void Stack::beginFunction(size_t nargs, size_t nlocals) {
  if (frame.operandStackSize != nargs) {
    runtimeError("expected {} arguments, but found operand stack of size {}",
                 nargs, frame.operandStackSize);
  }
  Value *newBase = frame.top;
  frame.operandStackSize = 0;
  frame.top += nargs;
  frameStack.push_back(frame);
  frame.base = newBase;
  frame.top = frame.base - nlocals;
  frame.nargs = nargs;
  frame.nlocals = nlocals;
  frame.operandStackSize = 0;
  frame.returnAddress = nextReturnAddress;
  // Fill with boxed zeros so that GC will skip these
  memset(frame.top, 1, frame.base - frame.top);
}

const char *Stack::endFunction() {
  if (isEmpty()) {
    runtimeError("no function to end");
  }
  if (frame.operandStackSize != 1) {
    runtimeError(
        "attempt to end function with operand stack size {}, expected 1",
        frame.operandStackSize);
  }
  Value top = *frame.top;
  const char *returnAddress = frame.returnAddress;
  frame = frameStack.back();
  frameStack.pop_back();
  pushIntOperand(top);
  return returnAddress;
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
    runtimeError(
        "expected a (boxed) number at the operand stack top, found {:#x}",
        operand);
  }
  return unboxInt(operand);
}

void Stack::popNOperands(size_t noperands) {
  if (frame.operandStackSize < noperands) {
    runtimeError("cannot pop {} operands because operand stack size is {}",
                 noperands, frame.operandStackSize);
  }
  frame.operandStackSize -= noperands;
  frame.top += noperands;
}

Value &Stack::accessLocal(ssize_t index) {
  if (index < 0 || index >= frame.nlocals) {
    runtimeError(
        "access local variable out of bounds: index {} is not in [0, {})",
        index, frame.nlocals);
  }
  return frame.base[-index - 1];
}

Value &Stack::accessArg(ssize_t index) {
  if (index < 0 || index >= frame.nargs) {
    runtimeError("access argument out of bounds: index {} is not in [0, {})",
                 index, frame.nargs);
  }
  return frame.base[frame.nargs - 1 - index];
}

void Stack::checkNonEmptyOperandStack() const {
  if (frame.operandStackSize == 0) {
    runtimeError("cannot pop from empty operand stack");
  }
}
