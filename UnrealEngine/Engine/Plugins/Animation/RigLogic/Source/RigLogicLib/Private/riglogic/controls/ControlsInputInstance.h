// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <functional>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rl4 {

class ControlsInputInstance {
    public:
        using Pointer = UniqueInstance<ControlsInputInstance>::PointerType;
        using Factory = std::function<Pointer(MemoryResource*)>;

    public:
        virtual ~ControlsInputInstance();
        virtual ArrayView<float> getGUIControlBuffer() = 0;
        virtual ArrayView<float> getInputBuffer() = 0;
        virtual ConstArrayView<float> getGUIControlBuffer() const = 0;
        virtual ConstArrayView<float> getInputBuffer() const = 0;

};

}  // namespace rl4
