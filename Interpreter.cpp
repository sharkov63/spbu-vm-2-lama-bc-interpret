#include "Interpreter.h"
#include "ByteFile.h"
#include "GlobalArea.h"
#include "Stack.h"
#include "fmt/format.h"
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

Stack stack;

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
  while (true) {
    try {
      step();
    } catch (std::runtime_error &e) {
      throw std::runtime_error(fmt::format("runtime error: {}", e.what()));
    }
  }
}

void Interpreter::step() {
  char byte = readByte();
  char high = (0xF0 & byte) >> 4;
  char low = 0x0F & byte;
  switch (high) {
  case 0x1: {
    switch (low) {
    // 0x18
    // DROP
    case 0x8: {
      stack.popOperand();
      return;
    }
    }
  }
  // ST
  case 0x4: {
    switch (low) {
    // 0x40 n:32
    // ST G(n)
    case 0x0: {
      int32_t globalIndex = readWord();
      int32_t &global = globalArea->accessGlobal(globalIndex);
      int32_t operand = stack.peakOperand();
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
      stack.pushOperand(Lread());
      return;
    }
  }
  }
  throw std::runtime_error(
      fmt::format("unsupported instruction code {:#x}", byte));
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
