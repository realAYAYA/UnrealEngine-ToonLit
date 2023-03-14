// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/conditionaltable/ConditionalTable.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/utils/Extd.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rl4 {

namespace {

const float clampMin = 0.0f;
const float clampMax = 1.0f;

}  // namespace

ConditionalTable::ConditionalTable(MemoryResource* memRes) :
    inputIndices{memRes},
    outputIndices{memRes},
    fromValues{memRes},
    toValues{memRes},
    slopeValues{memRes},
    cutValues{memRes},
    inputCount{},
    outputCount{} {
}

ConditionalTable::ConditionalTable(Vector<std::uint16_t>&& inputIndices_,
                                   Vector<std::uint16_t>&& outputIndices_,
                                   Vector<float>&& fromValues_,
                                   Vector<float>&& toValues_,
                                   Vector<float>&& slopeValues_,
                                   Vector<float>&& cutValues_,
                                   std::uint16_t inputCount_,
                                   std::uint16_t outputCount_) :
    inputIndices{std::move(inputIndices_)},
    outputIndices{std::move(outputIndices_)},
    fromValues{std::move(fromValues_)},
    toValues{std::move(toValues_)},
    slopeValues{std::move(slopeValues_)},
    cutValues{std::move(cutValues_)},
    inputCount{inputCount_},
    outputCount{outputCount_} {
}

std::uint16_t ConditionalTable::getInputCount() const {
    return inputCount;
}

std::uint16_t ConditionalTable::getOutputCount() const {
    return outputCount;
}

void ConditionalTable::calculate(const float* inputs, float* outputs, std::uint16_t chunkSize) const {
    std::fill_n(outputs, outputCount, 0.0f);

    for (std::uint16_t i = 0u; i < chunkSize; ++i) {
        const float inValue = inputs[inputIndices[i]];
        const float from = fromValues[i];
        const float to = toValues[i];
        if ((from < inValue) && (inValue <= to)) {
            const std::uint16_t outIndex = outputIndices[i];
            const float slope = slopeValues[i];
            const float cut = cutValues[i];
            outputs[outIndex] += (slope * inValue + cut);
        }
    }

    for (std::size_t i = 0ul; i < outputCount; ++i) {
        outputs[i] = extd::clamp(outputs[i], clampMin, clampMax);
    }
}

void ConditionalTable::calculate(const float* inputs, float* outputs) const {
    calculate(inputs, outputs, static_cast<std::uint16_t>(outputIndices.size()));
}

}  // namespace rl4
