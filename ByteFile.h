#pragma once

#include <memory>
#include <string>

namespace lama {

class ByteFile {
public:
  ByteFile() = default;
  ByteFile(std::unique_ptr<const char[]> data, size_t sizeBytes);

  static ByteFile load(std::string path);

  const char *getCode() const { return code; }

private:
  void init();

private:
  std::unique_ptr<const char[]> data;
  size_t sizeBytes;

  const char *stringTable;
  size_t stringTableSizeBytes;

  std::unique_ptr<int32_t[]> globalArea;
  size_t globalAreaSizeWords;

  const int32_t *publicSymbolTable;
  size_t publicSymbolsNum;

  const char *code;
  size_t codeSizeBytes;
};

} // namespace lama
