#pragma once

#include "Value.h"
#include <cstdlib>
#include <memory>

namespace lama {

class GlobalArea {
public:
  GlobalArea() = default;
  GlobalArea(size_t sizeWords);
  GlobalArea(GlobalArea &&) = default;
  GlobalArea &operator=(GlobalArea &&) = default;

  Value &accessGlobal(ssize_t index);

private:
  std::unique_ptr<Value[]> data;
  size_t sizeWords;
};

} // namespace lama
