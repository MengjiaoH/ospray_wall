
#pragma once
#include "ospcommon/vec.h"


void writePPM(const char *fileName,
              const ospcommon::vec2i *size,
              const uint32_t *pixel);

