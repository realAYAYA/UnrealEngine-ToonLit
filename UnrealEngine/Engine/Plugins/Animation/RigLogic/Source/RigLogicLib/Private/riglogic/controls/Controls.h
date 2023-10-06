// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/conditionaltable/ConditionalTable.h"
#include "riglogic/controls/ControlsInputInstance.h"
#include "riglogic/psdmatrix/PSDMatrix.h"

#include <cstdint>

namespace rl4 {

class Controls {
    public:
        using Pointer = UniqueInstance<Controls>::PointerType;

    public:
        Controls(ConditionalTable&& guiToRawMapping_, PSDMatrix&& psds_, ControlsInputInstance::Factory instanceFactory_);

        ControlsInputInstance::Pointer createInstance(MemoryResource* instanceMemRes) const;
        void mapGUIToRaw(ControlsInputInstance* instance) const;
        void mapRawToGUI(ControlsInputInstance* instance) const;
        void calculate(ControlsInputInstance* instance) const;

        template<class Archive>
        void serialize(Archive& archive) {
            archive(guiToRawMapping, psds);
        }

    private:
        ConditionalTable guiToRawMapping;
        PSDMatrix psds;
        ControlsInputInstance::Factory instanceFactory;

};

}  // namespace rl4
