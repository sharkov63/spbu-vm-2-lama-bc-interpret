#include "Interpreter.h"
#include "ByteFile.h"
#include "Error.h"
#include "GlobalArea.h"
#include "Stack.h"
#include "Value.h"
#include <iostream>

using namespace lama;

extern "C" {

void __gc_init();

void *__start_custom_data;
void *__stop_custom_data;

extern size_t __gc_stack_top, __gc_stack_bottom;

extern Value Lread();
extern int32_t Lwrite(Value boxedInt);
extern int32_t Llength(void *p);

extern void *Bstring(void *cstr);
}

enum VarDesignation {
  LOC_Global = 0x0,
  LOC_Local = 0x1,
  LOC_Arg = 0x2,
  LOC_Access = 0x3,
};

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
  /// \return true to continue, false to stop
  bool step();

  char readByte();
  int32_t readWord();

  Value &accessVar(char designation, int32_t index);

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
      if (!step())
        return;
    } catch (std::runtime_error &e) {
      runtimeError("runtime error at {:#x}: {}",
                   currentInstruction - byteFile.getCode(), e.what());
    }
  }
}

bool Interpreter::step() {
  // std::cerr << fmt::format("interpreting at {:#x}\n",
  //                          instructionPointer - byteFile.getCode());
  char byte = readByte();
  char high = (0xF0 & byte) >> 4;
  char low = 0x0F & byte;
  switch (high) {
  // BINOP
  case 0x0: {
    int32_t rhs = stack.popIntOperand();
    int32_t lhs = stack.popIntOperand();
    if ((low == BINOP_Div || low == BINOP_Mod) && rhs == 0)
      runtimeError("division by zero");
    int32_t result;
    switch (low) {
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
      runtimeError("undefined binary operator with code {:x}", low);
    }
    }
    stack.pushIntOperand(result);
    return true;
  }
  case 0x1: {
    switch (low) {
    // 0x10 n:32
    // CONST n
    case 0x0: {
      stack.pushIntOperand(readWord());
      return true;
    }
    // 0x11 s:32
    // STRING s
    case 0x1: {
      uint32_t offset = readWord();
      const char *cstr = byteFile.getStringAt(offset);
      Value string = reinterpret_cast<Value>(Bstring(const_cast<char *>(cstr)));
      stack.pushOperand(string);
      return true;
    }
    // 0x15 l:32
    // JMP l
    case 0x5: {
      uint32_t offset = readWord();
      instructionPointer = byteFile.getAddressFor(offset);
      return true;
    }
    // 0x16
    // END
    case 0x6: {
      const char *returnAddress = stack.endFunction();
      if (stack.isEmpty())
        return false;
      instructionPointer = returnAddress;
      return true;
    }
    // 0x18
    // DROP
    case 0x8: {
      stack.popOperand();
      return true;
    }
    }
    break;
  }
  // 0x2d n:32
  // LD
  case 0x2: {
    int32_t index = readWord();
    Value &var = accessVar(low, index);
    stack.pushOperand(var);
    return true;
  }
  // 0x4d n:32
  // ST
  case 0x4: {
    int32_t index = readWord();
    Value &var = accessVar(low, index);
    Value operand = stack.peakOperand();
    var = operand;
    return true;
  }
  case 0x5: {
    switch (low) {
    // 0x5x l:32
    // CJMPx l
    //   where x = 'z' | x = 'nz'
    case 0x0:
    case 0x1: {
      uint32_t offset = readWord();
      bool boolValue = stack.popIntOperand();
      if (boolValue == (bool)low)
        instructionPointer = byteFile.getAddressFor(offset);
      return true;
    }
    // 0x52 n:32 n:32
    // BEGIN nargs nlocals
    case 0x2: {
      uint32_t nargs = readWord();
      uint32_t nlocals = readWord();
      stack.beginFunction(nargs, nlocals);
      return true;
    }
    // 0x56 l:32 n:32
    // CALL address nargs
    case 0x6: {
      uint32_t offset = readWord();
      const char *address = byteFile.getAddressFor(offset);
      readWord();
      stack.setNextReturnAddress(instructionPointer);
      instructionPointer = address;
      return true;
    }
    // 0x5a n:32
    // LINE n
    case 0xa: {
      readWord();
      return true;
    }
    }
    break;
  }
  case 0x7: {
    switch (low) {
    // 0x70
    // CALL Lread
    case 0x0: {
      stack.pushOperand(Lread());
      return true;
    }
    // 0x71
    // CALL Lwrite
    case 0x1: {
      Lwrite(stack.popOperand());
      stack.pushIntOperand(0);
      return true;
    }
    // 0x72
    // CALL Llength
    case 0x2: {
      Value stringValue = stack.popOperand();
      Value lengthValue = Llength(reinterpret_cast<void *>(stringValue));
      stack.pushOperand(lengthValue);
      return true;
    }
    }
    break;
  }
  }
  runtimeError("unsupported instruction code {:#04x}", byte);
}

char Interpreter::readByte() { return *instructionPointer++; }

int32_t Interpreter::readWord() {
  int32_t word;
  memcpy(&word, instructionPointer, sizeof(int32_t));
  instructionPointer += sizeof(int32_t);
  return word;
}

Value &Interpreter::accessVar(char designation, int32_t index) {
  switch (designation) {
  case LOC_Global:
    return globalArea->accessGlobal(index);
  case LOC_Local:
    return stack.accessLocal(index);
  case LOC_Arg:
    return stack.accessArg(index);
  }
  runtimeError("unsupported variable designation {:#x}", designation);
}

void lama::interpret(ByteFile byteFile) {
  interpreter = Interpreter(std::move(byteFile));
  interpreter.run();
}
