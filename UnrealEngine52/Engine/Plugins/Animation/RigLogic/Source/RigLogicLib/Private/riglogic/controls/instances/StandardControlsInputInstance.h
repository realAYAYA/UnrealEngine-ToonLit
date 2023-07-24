// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/controls/ControlsInputInstance.h"

#include <cstdint>

namespace rl4 {

class StandardControlsInputInstance : public ControlsInputInstance {
    public:
        StandardControlsInputInstance(std::uint16_t guiControlCount,
                                      std::uint16_t rawControlCount,
                                      std::uint16_t psdControlCount,
                                      std::uint16_t mlControlCount,
                                      MemoryResource* memRes);

        ArrayView<float> getGUIControlBuffer() override;
        ArrayView<float> getInputBuffer() override;
        ConstArrayView<float> getGUIControlBuffer() const override;
        ConstArrayView<float> getInputBuffer() const override;

    private:
        Vector<float> guiControlBuffer;
        Vector<float> inputBuffer;

};

}  // namespace rl4
