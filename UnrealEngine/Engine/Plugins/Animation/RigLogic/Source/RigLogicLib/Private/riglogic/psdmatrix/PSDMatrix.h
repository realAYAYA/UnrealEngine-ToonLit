// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"

#include <cstddef>

namespace rl4 {

class PSDMatrix {
    public:
        explicit PSDMatrix(MemoryResource* memRes);
        PSDMatrix(std::uint16_t distinctPSDs_,
                  Vector<std::uint16_t>&& rowIndices_,
                  Vector<std::uint16_t>&& columnIndices_,
                  Vector<float>&& values_);

        std::uint16_t getDistinctPSDCount() const;
        void calculate(float* inputs, std::uint16_t rawControlCount) const;

        template<class Archive>
        void serialize(Archive& archive) {
            archive(distinctPSDs, rowIndices, columnIndices, values);
        }

    private:
        std::uint16_t distinctPSDs;
        Vector<std::uint16_t> rowIndices;
        Vector<std::uint16_t> columnIndices;
        Vector<float> values;
};

}  // namespace rl4
