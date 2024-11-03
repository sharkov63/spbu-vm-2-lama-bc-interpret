#include "Interpreter.h"
#include "ByteFile.h"
#include "GlobalArea.h"
#include "fmt/format.h"
#include <array>
#include <stdexcept>

using namespace lama;

extern "C" {

void __gc_init();

void *__start_custom_data;
void *__stop_custom_data;

extern size_t __gc_stack_top, __gc_stack_bottom;

extern int Lread();
}

namespace {

#define FILLER (1)

#define STACK_SIZE (1 << 20)

class Stack {
public:
  Stack();

  void beginFunction(size_t nlocals);
  void push(int32_t value);
  int32_t pop();

private:
  std::array<int32_t, STACK_SIZE> data;
  int32_t *base;
  int32_t *top;

  size_t operandStackSize = 0;

  std::vector<int32_t *> baseStack;
} stack;

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

void Stack::push(int32_t value) {
  ++operandStackSize;
  --top;
  *top = value;
}

int32_t Stack::pop() {
  if (operandStackSize == 0) {
    throw std::runtime_error("cannot pop from empty operand stack");
  }
  --operandStackSize;
  return *(top++);
}

class Interpreter {
public:
  Interpreter() = default;
  Interpreter(ByteFile byteFile);

  void run();

private:
  void step();

  char readByte();
  int32_t readWord();

private:
  ByteFile byteFile;
  std::shared_ptr<GlobalArea> globalArea;

  const char *instructionPointer;
} interpreter;

} // namespace

Interpreter::Interpreter(ByteFile byteFile)
    : byteFile(std::move(byteFile)),
      instructionPointer(this->byteFile.getCode()),
      globalArea(this->byteFile.getGlobalArea()) {}

void Interpreter::run() {
  __gc_init();
  while (true)
    step();
}

static void runtimeError(std::string message) {
  throw std::runtime_error("runtime error: " + message);
}

void Interpreter::step() {
  char byte = readByte();
  char high = (0xF0 & byte) >> 4;
  char low = 0x0F & byte;
  switch (high) {
  // ST
  case 0x4: {
    switch (low) {
    // 0x40 n:32
    // ST G(n)
    case 0x0: {
      int32_t globalIndex = readWord();
      int32_t &global = globalArea->accessGlobal(globalIndex);
      int32_t operand = stack.pop();
      global = operand;
      return;
    }
    }
  }
  case 0x5: {
    switch (low) {
    // 0x52 n:32 n:32
    // BEGIN nargs nlocals
    case 0x2: {
      readWord();
      int32_t nlocals = readWord();
      stack.beginFunction(nlocals);
      return;
    }
    // 0x5a n:32
    // LINE n
    case 0xa: {
      readWord();
      return;
    }
    }
  }
  case 0x7: {
    switch (low) {
    // 0x70
    // CALL Lread
    case 0x0:
      stack.push(Lread());
      return;
    }
  }
  }
  runtimeError(fmt::format("unsupported instruction code {:#x}", byte));
}

char Interpreter::readByte() { return *instructionPointer++; }
int32_t Interpreter::readWord() {
  int32_t word;
  memcpy(&word, instructionPointer, sizeof(int32_t));
  instructionPointer += sizeof(int32_t);
  return word;
}

void lama::interpret(ByteFile byteFile) {
  interpreter = Interpreter(std::move(byteFile));
  interpreter.run();
}
