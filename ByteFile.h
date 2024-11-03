#pragma once

#include "fmt/format.h"
#include <memory>
#include <string>

namespace lama {

class GlobalArea;

class ByteFile {
public:
  ByteFile() = default;
  ByteFile(std::unique_ptr<const char[]> data, size_t sizeBytes);

  static ByteFile load(std::string path);

  const char *getCode() const { return code; }

  const char *getAddressFor(size_t offset) const;

  std::shared_ptr<GlobalArea> getGlobalArea() { return globalArea; }

private:
  void init();

private:
  std::unique_ptr<const char[]> data;
  size_t sizeBytes;

  const char *stringTable;
  size_t stringTableSizeBytes;

  std::shared_ptr<GlobalArea> globalArea;

  const int32_t *publicSymbolTable;
  size_t publicSymbolsNum;

  const char *code;
  size_t codeSizeBytes;
};

} // namespace lama
