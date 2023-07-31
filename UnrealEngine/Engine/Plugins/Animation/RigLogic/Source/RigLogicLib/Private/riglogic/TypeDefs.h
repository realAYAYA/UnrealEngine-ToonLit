// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/types/Aliases.h"

#include <pma/PolyAllocator.h>
#include <pma/TypeDefs.h>
#include <pma/resources/AlignedMemoryResource.h>
#include <pma/resources/DefaultMemoryResource.h>

#include <cstddef>

namespace rl4 {

using namespace pma;

static constexpr std::size_t cacheLineAlignment = 64ul;

template<typename T>
using AlignedAllocator = PolyAllocator<T, cacheLineAlignment, AlignedMemoryResource>;

template<typename T>
using AlignedVector = Vector<T, AlignedAllocator<T> >;

template<typename T>
using AlignedMatrix = Vector<AlignedVector<T> >;

}  // namespace rl4
