#include "Interpreter.h"
#include "ByteFile.h"
#include "GlobalArea.h"
#include "Stack.h"
#include "Value.h"
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

enum Binop {
  BINOP_Add = 0x1,
  BINOP_Sub = 0x2,
  BINOP_Mul = 0x3,
  BINOP_Div = 0x4,
  BINOP_Mod = 0x5,
  BINOP_Lt = 0x6,
  BINOP_Leq = 0x7,
  BINOP_Gt = 0x8,
  BINOP_Geq = 0x9,
  BINOP_Eq = 0xa,
  BINOP_Neq = 0xb,
  BINOP_And = 0xc,
  BINOP_Or = 0xd,
};

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
    const char *currentInstruction = instructionPointer;
    try {
      step();
    } catch (std::runtime_error &e) {
      throw std::runtime_error(
          fmt::format("runtime error at {:#x}: {}",
                      currentInstruction - byteFile.getCode(), e.what()));
    }
  }
}

void Interpreter::step() {
  char byte = readByte();
  char high = (0xF0 & byte) >> 4;
  char low = 0x0F & byte;
  switch (high) {
  // BINOP
  case 0x0: {
    int32_t rhs = stack.popIntOperand();
    int32_t lhs = stack.popIntOperand();
    if ((low == BINOP_Div || low == BINOP_Mod) && rhs == 0)
      throw std::runtime_error("division by zero");
    int32_t result;
    switch (low) {
    // BINOP +
    case BINOP_Add: {
      result = lhs + rhs;
      break;
    }
    case BINOP_Sub: {
      result = lhs - rhs;
      break;
    }
    case BINOP_Mul: {
      result = lhs * rhs;
      break;
    }
    case BINOP_Div: {
      result = lhs / rhs;
      break;
    }
    case BINOP_Mod: {
      result = lhs % rhs;
      break;
    }
    case BINOP_Lt: {
      result = lhs < rhs;
      break;
    }
    case BINOP_Leq: {
      result = lhs <= rhs;
      break;
    }
    case BINOP_Gt: {
      result = lhs > rhs;
      break;
    }
    case BINOP_Geq: {
      result = lhs >= rhs;
      break;
    }
    case BINOP_Eq: {
      result = lhs == rhs;
      break;
    }
    case BINOP_Neq: {
      result = lhs != rhs;
      break;
    }
    case BINOP_And: {
      result = lhs && rhs;
      break;
    }
    case BINOP_Or: {
      result = lhs || rhs;
      break;
    }
    default: {
      throw std::runtime_error(
          fmt::format("undefined binary operator with code {:x}", low));
    }
    }
    stack.pushIntOperand(result);
    return;
  }
  case 0x1: {
    switch (low) {
    // 0x18
    // DROP
    case 0x8: {
      stack.popOperand();
      return;
    }
    }
    break;
  }
  // LD
  case 0x2: {
    switch (low) {
    // 0x20 n:32
    // LD G(n)
    case 0x0: {
      int32_t globalIndex = readWord();
      Value &global = globalArea->accessGlobal(globalIndex);
      stack.pushOperand(global);
      return;
    }
    }
    break;
  }
  // ST
  case 0x4: {
    switch (low) {
    // 0x40 n:32
    // ST G(n)
    case 0x0: {
      int32_t globalIndex = readWord();
      Value &global = globalArea->accessGlobal(globalIndex);
      Value operand = stack.peakOperand();
      global = operand;
      return;
    }
    }
    break;
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
    break;
  }
  case 0x7: {
    switch (low) {
    // 0x70
    // CALL Lread
    case 0x0:
      stack.pushOperand(Lread());
      return;
    }
    break;
  }
  }
  throw std::runtime_error(
      fmt::format("unsupported instruction code {:#04x}", byte));
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
