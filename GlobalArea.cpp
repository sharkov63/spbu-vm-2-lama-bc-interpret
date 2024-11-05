#include "GlobalArea.h"
#include "Error.h"

using namespace lama;

#define GLOBAL_AREA_SIZE 1024 * 1024

extern "C" {

Value __start_custom_data;
Value globalData[GLOBAL_AREA_SIZE];
Value __stop_custom_data;
}

static size_t sizeWords;

void lama::initGlobalArea(size_t sizeWords) {
  if (sizeWords > GLOBAL_AREA_SIZE) {
    runtimeError("Too large global area size = {}", sizeWords);
  }
  ::sizeWords = sizeWords;
  memset(globalData, 1, sizeof globalData);
  __start_custom_data = 1;
}

Value &lama::accessGlobal(int32_t index) {
  if (index < 0 || index >= sizeWords) {
    runtimeError(
        "access global variable out of bounds: index {} is not in [0, {})",
        index, sizeWords);
  }
  return globalData[index];
}
