// Copyright Epic Games, Inc. All Rights Reserved.

// *INDENT-OFF*
#ifdef RL_BUILD_WITH_ML_EVALUATOR

#include "riglogic/ml/cpu/OutputInstance.h"

#include <cassert>
#include <cstdint>

namespace rl4 {

namespace ml {

namespace cpu {

OutputInstance::OutputInstance(ConstArrayView<std::uint32_t> maxLayerOutputCounts, MemoryResource* memRes) :
    outputBuffer1{memRes},
    outputBuffer2{memRes},
    maskBuffer{maxLayerOutputCounts.size(), 1.0f, memRes} {

    outputBuffer1.resize(maxLayerOutputCounts.size());
    outputBuffer2.resize(maxLayerOutputCounts.size());
    for (std::size_t neuralNetIdx = 0ul; neuralNetIdx < maxLayerOutputCounts.size(); ++neuralNetIdx) {
        outputBuffer1[neuralNetIdx].resize(maxLayerOutputCounts[neuralNetIdx]);
        outputBuffer2[neuralNetIdx].resize(maxLayerOutputCounts[neuralNetIdx]);
    }
}

ArrayView<float> OutputInstance::getOutputBuffer1(std::uint32_t neuralNetIndex) {
    assert(neuralNetIndex < outputBuffer1.size());
    return ArrayView<float>{outputBuffer1[neuralNetIndex]};
}

ArrayView<float> OutputInstance::getOutputBuffer2(std::uint32_t neuralNetIndex) {
    assert(neuralNetIndex < outputBuffer2.size());
    return ArrayView<float>{outputBuffer2[neuralNetIndex]};
}

ArrayView<float> OutputInstance::getMaskBuffer() {
    return ArrayView<float>{maskBuffer};
}

ConstArrayView<float> OutputInstance::getMaskBuffer() const {
    return ConstArrayView<float>{maskBuffer};
}

}  // namespace cpu

}  // namespace ml

}  // namespace rl4

#endif  // RL_BUILD_WITH_ML_EVALUATOR
// *INDENT-ON*
