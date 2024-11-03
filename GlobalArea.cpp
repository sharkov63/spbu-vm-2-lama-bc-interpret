#include "GlobalArea.h"
#include "fmt/format.h"
#include <stdexcept>

using namespace lama;

//===----------------------------------------------------------------------===//
// GlobalArea
//===----------------------------------------------------------------------===//

GlobalArea::GlobalArea(size_t sizeWords)
    : data(new Value[sizeWords]), sizeWords(sizeWords) {}

Value &GlobalArea::accessGlobal(ssize_t index) {
  if (index < 0 || index >= sizeWords) {
    throw std::runtime_error(
        fmt::format("access out of global area bounds: {} is not in [0, {})",
                    index, sizeWords));
  }
  return data.get()[index];
}
