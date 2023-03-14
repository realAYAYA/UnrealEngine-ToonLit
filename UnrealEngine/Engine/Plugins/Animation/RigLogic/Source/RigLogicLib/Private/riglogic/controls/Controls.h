// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/conditionaltable/ConditionalTable.h"
#include "riglogic/psdmatrix/PSDMatrix.h"
#include "riglogic/types/Aliases.h"

#include <cstdint>

namespace rl4 {

class Controls {
    public:
        Controls(ConditionalTable&& guiToRawMapping_, PSDMatrix&& psds_);

        void mapGUIToRaw(ConstArrayView<float> guiValues, ArrayView<float> inputs) const;
        void calculate(ArrayView<float> inputs) const;

        template<class Archive>
        void serialize(Archive& archive) {
            archive(guiToRawMapping, psds);
        }

    private:
        ConditionalTable guiToRawMapping;
        PSDMatrix psds;

};

}  // namespace rl4
