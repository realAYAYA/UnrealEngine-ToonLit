// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/controls/Controls.h"

#include "riglogic/conditionaltable/ConditionalTable.h"
#include "riglogic/controls/ControlsInputInstance.h"
#include "riglogic/psdmatrix/PSDMatrix.h"

#include <cassert>
#include <cstdint>

namespace rl4 {

Controls::Controls(ConditionalTable&& guiToRawMapping_, PSDMatrix&& psds_, ControlsInputInstance::Factory instanceFactory_) :
    guiToRawMapping{std::move(guiToRawMapping_)},
    psds{std::move(psds_)},
    instanceFactory{instanceFactory_} {
}

ControlsInputInstance::Pointer Controls::createInstance(MemoryResource* instanceMemRes) const {
    return instanceFactory(instanceMemRes);
}

void Controls::mapGUIToRaw(ControlsInputInstance* instance) const {
    auto guiControlBuffer = instance->getGUIControlBuffer();
    assert(guiControlBuffer.size() == guiToRawMapping.getInputCount());
    auto inputBuffer = instance->getInputBuffer();
    guiToRawMapping.calculateForward(guiControlBuffer.data(), inputBuffer.data());
}

void Controls::mapRawToGUI(ControlsInputInstance* instance) const {
    auto guiControlBuffer = instance->getGUIControlBuffer();
    assert(guiControlBuffer.size() == guiToRawMapping.getInputCount());
    auto inputBuffer = instance->getInputBuffer();
    guiToRawMapping.calculateReverse(guiControlBuffer.data(), inputBuffer.data());
}

void Controls::calculate(ControlsInputInstance* instance) const {
    psds.calculate(instance->getInputBuffer().data(), guiToRawMapping.getOutputCount());
}

}  // namespace rl4
