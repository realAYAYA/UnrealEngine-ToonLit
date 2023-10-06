// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"

namespace rl4 {

struct LODSpec {
    Matrix<std::uint32_t> netIndicesPerLOD;
    std::uint32_t netCount;

    explicit LODSpec(MemoryResource* memRes) : netIndicesPerLOD{memRes}, netCount{} {
    }

    LODSpec(Matrix<std::uint32_t>&& netIndicesPerLOD_, std::uint32_t netCount_) :
        netIndicesPerLOD{std::move(netIndicesPerLOD_)},
        netCount{netCount_} {
    }

    template<class Archive>
    void serialize(Archive& archive) {
        archive(netIndicesPerLOD, netCount);
    }

};

}  // namespace rl4
