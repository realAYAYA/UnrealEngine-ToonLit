// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/psdmatrix/PSDMatrix.h"

#include "riglogic/TypeDefs.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <algorithm>
#include <cstddef>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rl4 {

namespace {

constexpr float maxControlValue = 1.0f;
constexpr std::size_t blockSize = 4ul;

}  // namespace

PSDMatrix::PSDMatrix(MemoryResource* memRes) :
    distinctPSDs{},
    rowIndices{memRes},
    columnIndices{memRes},
    values{memRes} {
}

PSDMatrix::PSDMatrix(std::uint16_t distinctPSDs_,
                     Vector<std::uint16_t>&& rowIndices_,
                     Vector<std::uint16_t>&& columnIndices_,
                     Vector<float>&& values_) :
    distinctPSDs{distinctPSDs_},
    rowIndices{std::move(rowIndices_)},
    columnIndices{std::move(columnIndices_)},
    values{std::move(values_)} {
}

std::uint16_t PSDMatrix::getDistinctPSDCount() const {
    return distinctPSDs;
}

void PSDMatrix::calculate(float* inputs, std::uint16_t rawControlCount) const {
    std::fill_n(inputs + rawControlCount, distinctPSDs, 1.0f);

    std::size_t alignedSize = values.size() - (values.size() % blockSize);
    for (std::size_t i = 0ul; i < alignedSize; i += blockSize) {
        const std::uint16_t row0 = rowIndices[i];
        const std::uint16_t row1 = rowIndices[i + 1ul];
        const std::uint16_t row2 = rowIndices[i + 2ul];
        const std::uint16_t row3 = rowIndices[i + 3ul];
        const std::uint16_t col0 = columnIndices[i];
        const std::uint16_t col1 = columnIndices[i + 1ul];
        const std::uint16_t col2 = columnIndices[i + 2ul];
        const std::uint16_t col3 = columnIndices[i + 3ul];
        const float weight0 = values[i];
        const float weight1 = values[i + 1ul];
        const float weight2 = values[i + 2ul];
        const float weight3 = values[i + 3ul];
        const float input0 = inputs[col0];
        const float input1 = inputs[col1];
        const float input2 = inputs[col2];
        const float input3 = inputs[col3];
        inputs[row0] *= weight0 * input0;
        inputs[row1] *= weight1 * input1;
        inputs[row2] *= weight2 * input2;
        inputs[row3] *= weight3 * input3;
    }

    for (std::size_t i = alignedSize; i < values.size(); ++i) {
        const std::uint16_t row = rowIndices[i];
        const std::uint16_t col = columnIndices[i];
        const float weight = values[i];
        const float input = inputs[col];
        inputs[row] *= weight * input;
    }

    auto psds = inputs + rawControlCount;
    for (std::size_t i = 0ul; i < distinctPSDs; ++i) {
        psds[i] = std::min(maxControlValue, psds[i]);
    }
}

}  // namespace rl4
