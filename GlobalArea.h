#pragma once

#include "Value.h"

namespace lama {

void initGlobalArea(size_t sizeWords);
Value &accessGlobal(int32_t index);

} // namespace lama
