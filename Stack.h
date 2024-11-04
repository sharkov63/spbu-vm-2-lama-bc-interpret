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

  void beginFunction(size_t nargs, size_t nlocals);
  /// \return return address; invalid if ended main
  const char *endFunction();

  void pushOperand(Value operand);
  Value peakOperand() const;
  Value popOperand();
  void pushIntOperand(int32_t operand);
  int32_t popIntOperand();
  void popNOperands(size_t noperands);

  bool isEmpty() const { return frameStack.empty(); }
  bool isNotEmpty() const { return !isEmpty(); }

  Value &accessLocal(ssize_t index);
  Value &accessArg(ssize_t index);

  void setNextReturnAddress(const char *address) {
    nextReturnAddress = address;
  }

  Value *getTop() { return frame.top; }
  Value *getBottom() { return data.end(); }
  size_t getOperandStackSize() const { return frame.operandStackSize; }

private:
  void checkNonEmptyOperandStack() const;

  struct Frame {
    Value *base;
    Value *top;
    size_t nargs;
    size_t nlocals;
    size_t operandStackSize = 0;
    const char *returnAddress;
  };

private:
  std::array<Value, STACK_SIZE> data;
  Frame frame;
  std::vector<Frame> frameStack;
  const char *nextReturnAddress;
};

} // namespace lama
