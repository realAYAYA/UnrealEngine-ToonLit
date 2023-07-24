// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "trimd/Macros.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <cstring>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace trimd {

template<typename TTarget, typename TSource>
FORCE_INLINE TTarget bitcast(TSource source) {
    static_assert(sizeof(TTarget) == sizeof(TSource), "Target and source must be of equal size.");
    TTarget target;
    std::memcpy(&target, &source, sizeof(TSource));
    return target;
}

}  // namespace trimd
