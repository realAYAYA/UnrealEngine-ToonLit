// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/controls/Controls.h"

#include "riglogic/conditionaltable/ConditionalTable.h"
#include "riglogic/psdmatrix/PSDMatrix.h"

#include <cassert>
#include <cstdint>

namespace rl4 {

Controls::Controls(ConditionalTable&& guiToRawMapping_, PSDMatrix&& psds_) :
    guiToRawMapping{std::move(guiToRawMapping_)},
    psds{std::move(psds_)} {
}

void Controls::mapGUIToRaw(ConstArrayView<float> guiValues, ArrayView<float> inputs) const {
    assert(guiValues.size() == guiToRawMapping.getInputCount());
    guiToRawMapping.calculate(guiValues.data(), inputs.data());
}

void Controls::calculate(ArrayView<float> inputs) const {
    psds.calculate(inputs.data(), guiToRawMapping.getOutputCount());
}

}  // namespace rl4
