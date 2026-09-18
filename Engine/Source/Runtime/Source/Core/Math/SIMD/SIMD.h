#pragma once

// Lumina::SIMD -- thin x86 intrinsic wrappers (VFloat4/VFloat8) for hand-vectorized hot
// loops; not a TVec replacement. LoadAligned/StoreAligned buffers must be kAlignment-aligned.

#include "SIMDConfig.h"
#include "ArrayOps.h"
#include "PackHalf.h"
#include "VFloat4.h"
#include "VFloat8.h"
#include "VQuat4.h"
#include "VQuat8.h"
