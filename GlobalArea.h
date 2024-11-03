#pragma once

#include <cstdlib>
#include <memory>

namespace lama {

class GlobalArea {
public:
  GlobalArea() = default;
  GlobalArea(size_t sizeWords);
  GlobalArea(GlobalArea &&) = default;
  GlobalArea &operator=(GlobalArea &&) = default;

  int32_t &accessGlobal(ssize_t index);

private:
  std::unique_ptr<int32_t[]> data;
  size_t sizeWords;
};

} // namespace lama
