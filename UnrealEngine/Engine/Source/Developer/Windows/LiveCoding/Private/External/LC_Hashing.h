// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "xxhash.h"
// BEGIN EPIC MOD
#include "LC_Platform.h"
#include "LC_Foundation_Windows.h"
// END EPIC MOD

namespace Hashing
{
	LC_ALWAYS_INLINE uint32_t Hash32(const void* input, size_t length, uint32_t seed)
	{
		return XXH32(input, length, seed);
	}
}
