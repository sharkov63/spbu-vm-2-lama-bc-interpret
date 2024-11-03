#include "Interpreter.h"
#include "ByteFile.h"
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

private:
  std::array<int32_t, STACK_SIZE> data;
  int32_t *base;
  int32_t *top;

  std::vector<int32_t *> baseStack;
} stack;

Stack::Stack() {
  top = data.end();
  // __gc_stack_bottom = (size_t)top;
  // __gc_stack_top = __gc_stack_bottom;
}

void Stack::beginFunction(size_t nlocals) {
  baseStack.push_back(base);
  base = top;
  top -= nlocals;
  // Fill with boxed zeros so that GC will skip these
  memset(top, 1, base - top);
}

void Stack::push(int32_t value) {
  --top;
  *top = value;
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

  const char *instructionPointer;
} interpreter;

} // namespace

Interpreter::Interpreter(ByteFile byteFile)
    : byteFile(std::move(byteFile)), instructionPointer(byteFile.getCode()) {}

void Interpreter::run() {
  __gc_init();
  while (true)
    step();
}

static void runtimeError(std::string message) {
  throw std::runtime_error("runtime error: " + message);
}

void Interpreter::step() {
  char byte = *instructionPointer++;
  char high = (0xF0 & byte) >> 4;
  char low = 0x0F & byte;
  switch (high) {
  case 5:
    switch (low) {
    // 0x52 n:32 n:32
    // BEGIN nargs nlocals
    case 2:
      readWord();
      int32_t nlocals = readWord();
      stack.beginFunction(nlocals);
      return;
    }
  case 7:
    switch (low) {
    // 0x70
    // CALL Lread
    case 0:
      stack.push(Lread());
      return;
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
  Interpreter(std::move(byteFile)).run();
}
