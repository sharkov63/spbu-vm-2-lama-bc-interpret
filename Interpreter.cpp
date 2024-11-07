#include "Interpreter.h"
#include "ByteFile.h"
#include "Error.h"
#include "Value.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <vector>

using namespace lama;

extern "C" {

extern Value __start_custom_data;
extern Value __stop_custom_data;
extern Value *__gc_stack_top;
extern Value *__gc_stack_bottom;

void __gc_init();

extern Value Lread();
extern int32_t Lwrite(Value boxedInt);
extern int32_t Llength(void *p);
extern void *Lstring(void *p);

extern void *Belem(void *p, int i);
extern void *Bstring(void *cstr);
extern void *Bsta(void *v, int i, void *x);
extern void *Barray(int bn, ...);
extern void *Barray_(void *stack_top, int n);
extern int LtagHash(char *tagString);
extern void *Bsexp(int bn, ...);
extern void *Bsexp_(void *stack_top, int n);
extern int Btag(void *d, int t, int n);
[[noreturn]] extern void Bmatch_failure(void *v, char *fname, int line,
                                        int col);
extern void *Bclosure(int bn, void *entry, ...);
extern void *Bclosure_(void *stack_top, int n, void *entry);
extern int Bstring_patt(void *x, void *y);
extern int Bclosure_tag_patt(void *x);
extern int Bboxed_patt(void *x);
extern int Bunboxed_patt(void *x);
extern int Barray_tag_patt(void *x);
extern int Bstring_tag_patt(void *x);
extern int Bsexp_tag_patt(void *x);
extern int Barray_patt(void *d, int n);
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

enum InstCode {
  I_CONST = 0x10,
  I_STRING = 0x11,
  I_SEXP = 0x12,
  I_STA = 0x14,
  I_JMP = 0x15,
  I_END = 0x16,
  I_DROP = 0x18,
  I_DUP = 0x19,
  I_ELEM = 0x1b,
};

static void initGlobalArea() {
  for (Value *p = &__start_custom_data; p < &__stop_custom_data; ++p)
    *p = 1;
}

static Value &accessGlobal(uint32_t index) {
  return (&__start_custom_data)[index];
}

#define STACK_SIZE (1 << 20)

namespace {

struct Stack {

  static void init() {
    __gc_stack_bottom = data.end();
    frame.base = __gc_stack_bottom;
    // Two arguments to main: argc and argv
    __gc_stack_top = __gc_stack_bottom - 3;
    frame.operandStackSize = 2;
  }

  static size_t getOperandStackSize() { return frame.operandStackSize; }
  static bool isEmpty() { return frameStack.empty(); }
  static bool isNotEmpty() { return !isEmpty(); }
  static Value getClosure();

  static Value &accessLocal(ssize_t index);
  static Value &accessArg(ssize_t index);

  static void pushOperand(Value value) {
    ++frame.operandStackSize;
    *top() = value;
    --top();
  }
  static Value peakOperand() {
    checkNonEmptyOperandStack();
    return top()[1];
  }
  static Value popOperand() {
    checkNonEmptyOperandStack();
    --frame.operandStackSize;
    ++top();
    return *top();
  }
  static void popNOperands(size_t noperands) {
    if (frame.operandStackSize < noperands) {
      runtimeError("cannot pop {} operands because operand stack size is {}",
                   noperands, frame.operandStackSize);
    }
    frame.operandStackSize -= noperands;
    top() += noperands;
  }

  static void pushIntOperand(int32_t operand) { pushOperand(boxInt(operand)); }
  static int32_t popIntOperand() {
    Value operand = popOperand();
    if (!valueIsInt(operand)) {
      runtimeError(
          "expected a (boxed) number at the operand stack top, found {:#x}",
          operand);
    }
    return unboxInt(operand);
  }

  static void beginFunction(size_t nargs, size_t nlocals);
  static const char *endFunction();

  static void setNextReturnAddress(const char *address) {
    nextReturnAddress = address;
  }
  static void setNextIsClosure(bool isClousre) { nextIsClosure = isClousre; }

  static Value *&top() { return __gc_stack_top; }

private:
  static void checkNonEmptyOperandStack() {
    if (frame.operandStackSize == 0) {
      runtimeError("cannot pop from empty operand stack");
    }
  }

private:
  static std::array<Value, STACK_SIZE> data;

  struct Frame {
    Value *base;
    Value *top;
    size_t nargs;
    size_t nlocals;
    size_t operandStackSize = 0;
    const char *returnAddress;
  };

  static Frame frame;
  static std::vector<Frame> frameStack;

  static const char *nextReturnAddress;
  static bool nextIsClosure;
};

} // namespace

std::array<Value, STACK_SIZE> Stack::data;

Stack::Frame Stack::frame;
std::vector<Stack::Frame> Stack::frameStack;
const char *Stack::nextReturnAddress;
bool Stack::nextIsClosure;

Value Stack::getClosure() { return frame.base[frame.nargs]; }

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
void Stack::beginFunction(size_t nargs, size_t nlocals) {
  size_t noperands = nargs + nextIsClosure;
  if (frame.operandStackSize < noperands) {
    runtimeError("expected {} operands, but found only {}", noperands,
                 frame.operandStackSize);
  }
  Value *newBase = top() + 1;
  frame.operandStackSize -= noperands;
  frame.top = newBase + noperands - 1;
  frameStack.push_back(frame);
  frame.base = newBase;
  top() = newBase - nlocals - 1;
  frame.nargs = nargs;
  frame.nlocals = nlocals;
  frame.operandStackSize = 0;
  frame.returnAddress = nextReturnAddress;
  // Fill with some boxed values so that GC will skip these
  memset(top() + 1, 1, (char *)frame.base - (char *)(top() + 1));
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
  const char *returnAddress = frame.returnAddress;
  Value ret = peakOperand();
  frame = frameStack.back();
  frameStack.pop_back();
  top() = frame.top;
  pushOperand(ret);
  return returnAddress;
}

static Value renderToString(Value value) {
  return reinterpret_cast<Value>(Lstring(reinterpret_cast<void *>(value)));
}

static Value createString(const char *cstr) {
  return reinterpret_cast<Value>(Bstring(const_cast<char *>(cstr)));
}

static Value createArray(size_t nargs) {
  return reinterpret_cast<Value>(Barray_(Stack::top() + 1, nargs));
}

static Value createSexp(size_t nargs) {
  return reinterpret_cast<Value>(Bsexp_(Stack::top() + 1, nargs));
}

static Value createClosure(const char *entry, size_t nvars) {
  return reinterpret_cast<Value>(
      Bclosure_(Stack::top() + 1, nvars, const_cast<char *>(entry)));
}

static const char unknownFile[] = "<unknown file>";

namespace {

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

  const char *instructionPointer;
  const char *codeEnd;
} interpreter;

} // namespace

Interpreter::Interpreter(ByteFile byteFile)
    : byteFile(std::move(byteFile)),
      instructionPointer(this->byteFile.getCode()),
      codeEnd(instructionPointer + this->byteFile.getCodeSizeBytes()) {}

void Interpreter::run() {
  __gc_init();
  Stack::init();
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
  // if (Stack::getOperandStackSize()) {
  //   std::cerr << fmt::format("operand stack:\n");
  //   for (int i = 0; i < Stack::getOperandStackSize(); ++i) {
  //     Value element = Stack::top()[i + 1];
  //     std::cerr << fmt::format("operand {}, raw {:#x}\n", i,
  //     (unsigned)element);
  //   }
  // }
  // std::cerr << fmt::format("stack region is ({}, {})\n",
  // fmt::ptr(__gc_stack_top), fmt::ptr(__gc_stack_bottom));
  unsigned char byte = readByte();
  unsigned char high = (0xF0 & byte) >> 4;
  unsigned char low = 0x0F & byte;
  switch (high) {
  // BINOP
  case 0x0: {
    if (low == BINOP_Eq) {
      Value rhs = Stack::popOperand();
      Value lhs = Stack::popOperand();
      Value result = boxInt(lhs == rhs);
      Stack::pushOperand(result);
      return true;
    }

    int32_t rhs = Stack::popIntOperand();
    int32_t lhs = Stack::popIntOperand();
    if ((low == BINOP_Div || low == BINOP_Mod) && rhs == 0)
      runtimeError("division by zero");
    int32_t result;
    switch (low) {
#define CASE(code, op)                                                         \
  case code: {                                                                 \
    result = lhs op rhs;                                                       \
    break;                                                                     \
  }
      CASE(BINOP_Add, +)
      CASE(BINOP_Sub, -)
      CASE(BINOP_Mul, *)
      CASE(BINOP_Div, /)
      CASE(BINOP_Mod, %)
      CASE(BINOP_Lt, <)
      CASE(BINOP_Leq, <=)
      CASE(BINOP_Gt, >)
      CASE(BINOP_Geq, >=)
      CASE(BINOP_Neq, !=)
      CASE(BINOP_And, &&)
      CASE(BINOP_Or, ||)
#undef CASE
    default: {
      runtimeError("undefined binary operator with code {:x}", low);
    }
    }
    Stack::pushIntOperand(result);
    return true;
  }
  case 0x1: {
    switch (byte) {
    case I_CONST: {
      Stack::pushIntOperand(readWord());
      return true;
    }
    case I_STRING: {
      uint32_t offset = readWord();
      const char *cstr = byteFile.getStringAt(offset);
      Value string = createString(cstr);
      Stack::pushOperand(string);
      return true;
    }
    case I_SEXP: {
      Value stringOffset = readWord();
      uint32_t nargs = readWord();
      if (Stack::getOperandStackSize() < nargs) {
        runtimeError("cannot construct sexp of {} elements: operand stack "
                     "size is only {}",
                     nargs, Stack::getOperandStackSize());
      }
      const char *string = byteFile.getStringAt(stringOffset);
      Value tagHash = LtagHash(const_cast<char *>(string));
      std::reverse(Stack::top() + 1, Stack::top() + nargs + 1);
      Stack::pushOperand(0);
      Value *base = Stack::top() + 1;
      for (int i = 0; i < nargs; ++i) {
        base[i] = base[i + 1];
      }
      base[nargs] = tagHash;

      Value sexp = createSexp(nargs);

      Stack::popNOperands(nargs + 1);
      Stack::pushOperand(sexp);
      return true;
    }
    case I_STA: {
      Value value = Stack::popOperand();
      Value index = Stack::popOperand();
      Value container = Stack::popOperand();
      Value result =
          reinterpret_cast<Value>(Bsta(reinterpret_cast<void *>(value), index,
                                       reinterpret_cast<void *>(container)));
      Stack::pushOperand(result);
      return true;
    }
    case I_JMP: {
      uint32_t offset = readWord();
      instructionPointer = byteFile.getAddressFor(offset);
      return true;
    }
    case I_END: {
      const char *returnAddress = Stack::endFunction();
      if (Stack::isEmpty())
        return false;
      instructionPointer = returnAddress;
      return true;
    }
    case I_DROP: {
      Stack::popOperand();
      return true;
    }
    case I_DUP: {
      Stack::pushOperand(Stack::peakOperand());
      return true;
    }
    case I_ELEM: {
      Value index = Stack::popOperand();
      Value container = Stack::popOperand();
      Value element = reinterpret_cast<Value>(
          Belem(reinterpret_cast<void *>(container), index));
      Stack::pushOperand(element);
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
    Stack::pushOperand(var);
    return true;
  }
  // 0x3d n:32
  // LDA
  case 0x3: {
    int32_t index = readWord();
    Value *address = &accessVar(low, index);
    Stack::pushOperand(reinterpret_cast<Value>(address));
    Stack::pushOperand(reinterpret_cast<Value>(address));
    return true;
  }
  // 0x4d n:32
  // ST
  case 0x4: {
    int32_t index = readWord();
    Value &var = accessVar(low, index);
    Value operand = Stack::peakOperand();
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
      bool boolValue = Stack::popIntOperand();
      if (boolValue == (bool)low)
        instructionPointer = byteFile.getAddressFor(offset);
      return true;
    }
    // 0x5c n:32 n:32
    // BEGIN nargs nlocals
    //   0x52 - without closure
    //   0x53 - with closure
    case 0x2:
    case 0x3: {
      uint32_t nargs = readWord();
      uint32_t nlocals = readWord();
      bool isClosure = low == 0x3;
      Stack::beginFunction(nargs, nlocals);
      return true;
    }
    // 0x54 l:32 n:32 d*:32 *
    // CLOSURE
    case 0x4: {
      uint32_t entryOffset = readWord();
      uint32_t n = readWord();

      const char *entry = byteFile.getAddressFor(entryOffset);

      std::vector<Value> values;
      for (int i = 0; i < n; ++i) {
        char designation = readByte();
        uint32_t index = readWord();
        Value value = accessVar(designation, index);
        values.push_back(value);
      }
      std::reverse(values.begin(), values.end());
      for (Value value : values)
        Stack::pushOperand(value);

      Value closure = createClosure(entry, n);

      Stack::popNOperands(n);
      Stack::pushOperand(closure);
      return true;
    }
    // 0x55 n:32
    // CALLC nargs
    case 0x5: {
      uint32_t nargs = readWord();
      if (Stack::getOperandStackSize() < nargs + 1) {
        runtimeError("cannot call closure with {} args: operand stack size is "
                     "too small ({})",
                     nargs, Stack::getOperandStackSize());
      }
      Value closure = Stack::top()[nargs + 1];
      const char *entry = *reinterpret_cast<const char **>(closure);
      Stack::setNextReturnAddress(instructionPointer);
      Stack::setNextIsClosure(true);
      instructionPointer = entry;
      return true;
    }
    // 0x56 l:32 n:32
    // CALL address nargs
    case 0x6: {
      uint32_t offset = readWord();
      const char *address = byteFile.getAddressFor(offset);
      readWord();
      Stack::setNextReturnAddress(instructionPointer);
      Stack::setNextIsClosure(false);
      instructionPointer = address;
      return true;
    }
    // 0x57 s:32 n:32
    // TAG s n
    case 0x7: {
      uint32_t stringOffset = readWord();
      uint32_t nargs = readWord();
      const char *string = byteFile.getStringAt(stringOffset);
      Value tag = LtagHash(const_cast<char *>(string));
      Value target = Stack::popOperand();

      Value result = Btag((void *)target, tag, boxInt(nargs));

      Stack::pushOperand(result);
      return true;
    }
    // 0x58 n:32
    // ARRAY n
    case 0x8: {
      uint32_t nelems = readWord();
      Value array = Stack::popOperand();
      Value result =
          Barray_patt(reinterpret_cast<void *>(array), boxInt(nelems));
      Stack::pushOperand(result);
      return true;
    }
    // 0x59 l:32 c:32
    // FAIL l c
    case 0x9: {
      uint32_t line = readWord();
      uint32_t col = readWord();
      Value v = Stack::popOperand();
      Bmatch_failure((void *)v, const_cast<char *>(unknownFile), line,
                     col); // noreturn
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
  // 0x6p
  // PATT p
  case 0x6: {
    switch (low) {
    // 0x60
    // PATT StrCmp
    case 0x0: {
      Value x = Stack::popOperand();
      Value y = Stack::popOperand();
      Value result = Bstring_patt(reinterpret_cast<void *>(x),
                                  reinterpret_cast<void *>(y));
      Stack::pushOperand(result);
      return true;
    }
    // 0x61
    // PATT String
    case 0x1: {
      Value operand = Stack::popOperand();
      Value result = Bstring_tag_patt(reinterpret_cast<void *>(operand));
      Stack::pushOperand(result);
      return true;
    }
    // 0x62
    // PATT Array
    case 0x2: {
      Value operand = Stack::popOperand();
      Value result = Barray_tag_patt(reinterpret_cast<void *>(operand));
      Stack::pushOperand(result);
      return true;
    }
    // 0x63
    // PATT Sexp
    case 0x3: {
      Value operand = Stack::popOperand();
      Value result = Bsexp_tag_patt(reinterpret_cast<void *>(operand));
      Stack::pushOperand(result);
      return true;
    }
    // 0x64
    // PATT Boxed
    case 0x4: {
      Value operand = Stack::popOperand();
      Value result = Bboxed_patt(reinterpret_cast<void *>(operand));
      Stack::pushOperand(result);
      return true;
    }
    // 0x65
    // PATT UnBoxed
    case 0x5: {
      Value operand = Stack::popOperand();
      Value result = Bunboxed_patt(reinterpret_cast<void *>(operand));
      Stack::pushOperand(result);
      return true;
    }
    // 0x66
    // PATT Closure
    case 0x6: {
      Value operand = Stack::popOperand();
      Value result = Bclosure_tag_patt(reinterpret_cast<void *>(operand));
      Stack::pushOperand(result);
      return true;
    }
    default: {
      runtimeError("unsupported pattern kind {:#x}", low);
    }
    }
    break;
  }
  case 0x7: {
    switch (low) {
    // 0x70
    // CALL Lread
    case 0x0: {
      Stack::pushOperand(Lread());
      return true;
    }
    // 0x71
    // CALL Lwrite
    case 0x1: {
      Lwrite(Stack::popOperand());
      Stack::pushIntOperand(0);
      return true;
    }
    // 0x72
    // CALL Llength
    case 0x2: {
      Value string = Stack::popOperand();
      Value length = Llength(reinterpret_cast<void *>(string));
      Stack::pushOperand(length);
      return true;
    }
    // 0x73
    // CALL Lstring
    case 0x3: {
      Value operand = Stack::popOperand();
      Value rendered = renderToString(operand);
      Stack::pushOperand(rendered);
      return true;
    }
    // 0x74 n:32
    // CALL Barray n:32
    case 0x4: {
      uint32_t nargs = readWord();
      if (Stack::getOperandStackSize() < nargs) {
        runtimeError("cannot construct array of {} elements: operand stack "
                     "size is only {}",
                     nargs, Stack::getOperandStackSize());
      }
      std::reverse(Stack::top() + 1, Stack::top() + nargs + 1);
      Value array = createArray(nargs);
      Stack::popNOperands(nargs);
      Stack::pushOperand(array);
      return true;
    }
    }
    break;
  }
  }
  runtimeError("unsupported instruction code {:#04x}", byte);
}

char Interpreter::readByte() {
  if (instructionPointer > codeEnd - 1)
    runtimeError("unexpected end of bytecode, expected a byte");
  return *instructionPointer++;
}

int32_t Interpreter::readWord() {
  if (instructionPointer > codeEnd - 4)
    runtimeError("unexpected end of bytecode, expected a word");
  int32_t word;
  memcpy(&word, instructionPointer, sizeof(int32_t));
  instructionPointer += sizeof(int32_t);
  return word;
}

Value &Interpreter::accessVar(char designation, int32_t index) {
  switch (designation) {
  case LOC_Global:
    return accessGlobal(index);
  case LOC_Local:
    return Stack::accessLocal(index);
  case LOC_Arg:
    return Stack::accessArg(index);
  case LOC_Access:
    Value *closure = reinterpret_cast<Value *>(Stack::getClosure());
    return closure[index + 1];
  }
  runtimeError("unsupported variable designation {:#x}", designation);
}

void lama::interpret(ByteFile byteFile) {
  initGlobalArea();
  interpreter = Interpreter(std::move(byteFile));
  interpreter.run();
}
