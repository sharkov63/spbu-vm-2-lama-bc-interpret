#include "GlobalArea.h"
#include "Error.h"

using namespace lama;

//===----------------------------------------------------------------------===//
// GlobalArea
//===----------------------------------------------------------------------===//

GlobalArea::GlobalArea(size_t sizeWords)
    : data(new Value[sizeWords]), sizeWords(sizeWords) {}

Value &GlobalArea::accessGlobal(int32_t index) {
  if (index < 0 || index >= sizeWords) {
    runtimeError(
        "access global variable out of bounds: index {} is not in [0, {})",
        index, sizeWords);
  }
  return data.get()[index];
}
