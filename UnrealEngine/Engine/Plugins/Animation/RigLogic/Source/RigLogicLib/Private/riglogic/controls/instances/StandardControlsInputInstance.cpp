// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/controls/instances/StandardControlsInputInstance.h"

#include <cstdint>
#include <cstddef>

namespace rl4 {

StandardControlsInputInstance::StandardControlsInputInstance(std::uint16_t guiControlCount,
                                                             std::uint16_t rawControlCount,
                                                             std::uint16_t psdControlCount,
                                                             std::uint16_t mlControlCount,
                                                             MemoryResource* memRes) :
    guiControlBuffer{guiControlCount, {}, memRes},
    inputBuffer{static_cast<std::size_t>(rawControlCount + psdControlCount + mlControlCount), {}, memRes} {
}

ArrayView<float> StandardControlsInputInstance::getGUIControlBuffer() {
    return ArrayView<float>{guiControlBuffer};
}

ArrayView<float> StandardControlsInputInstance::getInputBuffer() {
    return ArrayView<float>{inputBuffer};
}

ConstArrayView<float> StandardControlsInputInstance::getGUIControlBuffer() const {
    return ConstArrayView<float>{guiControlBuffer};
}

ConstArrayView<float> StandardControlsInputInstance::getInputBuffer() const {
    return ConstArrayView<float>{inputBuffer};
}

}  // namespace rl4
