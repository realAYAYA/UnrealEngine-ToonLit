// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"

#include <cstddef>

namespace rl4 {

class ConditionalTable {
    public:
        explicit ConditionalTable(MemoryResource* memRes);
        ConditionalTable(Vector<std::uint16_t>&& inputIndices_,
                         Vector<std::uint16_t>&& outputIndices_,
                         Vector<float>&& fromValues_,
                         Vector<float>&& toValues_,
                         Vector<float>&& slopeValues_,
                         Vector<float>&& cutValues_,
                         std::uint16_t inputCount_,
                         std::uint16_t outputCount_);

        std::uint16_t getInputCount() const;
        std::uint16_t getOutputCount() const;
        void calculate(const float* inputs, float* outputs) const;
        void calculate(const float* inputs, float* outputs, std::uint16_t chunkSize) const;

        template<class Archive>
        void serialize(Archive& archive) {
            archive(inputIndices,
                    outputIndices,
                    fromValues,
                    toValues,
                    slopeValues,
                    cutValues,
                    inputCount,
                    outputCount);
        }

    private:
        Vector<std::uint16_t> inputIndices;
        Vector<std::uint16_t> outputIndices;
        Vector<float> fromValues;
        Vector<float> toValues;
        Vector<float> slopeValues;
        Vector<float> cutValues;
        std::uint16_t inputCount;
        std::uint16_t outputCount;
};

}  // namespace rl4
