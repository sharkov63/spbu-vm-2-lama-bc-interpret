#include "GlobalArea.h"
#include "Error.h"

using namespace lama;

extern "C" {

void *__start_custom_data;
void *__stop_custom_data;
}

//===----------------------------------------------------------------------===//
// GlobalArea
//===----------------------------------------------------------------------===//

GlobalArea::GlobalArea(size_t sizeWords)
    : data(new Value[sizeWords]), sizeWords(sizeWords) {
  // Fill with some boxed values so that GC will skip these
  memset(data.get(), 1, sizeWords * sizeof(Value));
  __start_custom_data = data.get();
  __stop_custom_data = data.get() + sizeWords;
}

Value &GlobalArea::accessGlobal(int32_t index) {
  if (index < 0 || index >= sizeWords) {
    runtimeError(
        "access global variable out of bounds: index {} is not in [0, {})",
        index, sizeWords);
  }
  return data.get()[index];
}
