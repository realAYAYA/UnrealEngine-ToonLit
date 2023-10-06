// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#ifdef RL_BUILD_WITH_ML_EVALUATOR

#include "riglogic/TypeDefs.h"
#include "riglogic/ml/MachineLearnedBehaviorOutputInstance.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <functional>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rl4 {

namespace ml {

namespace cpu {

class OutputInstance : public MachineLearnedBehaviorOutputInstance {
    public:
        using Factory = std::function<Pointer(ConstArrayView<std::uint32_t>, MemoryResource*)>;

    public:
        OutputInstance(ConstArrayView<std::uint32_t> maxLayerOutputCounts, MemoryResource* memRes);
        ArrayView<float> getOutputBuffer1(std::uint32_t neuralNetIndex);
        ArrayView<float> getOutputBuffer2(std::uint32_t neuralNetIndex);
        ArrayView<float> getMaskBuffer() override;
        ConstArrayView<float> getMaskBuffer() const override;

    private:
        Vector<AlignedVector<float> > outputBuffer1;
        Vector<AlignedVector<float> > outputBuffer2;
        Vector<float> maskBuffer;

};

}  // namespace cpu

}  // namespace ml

}  // namespace rl4

#endif  // RL_BUILD_WITH_ML_EVALUATOR
// *INDENT-ON*
