// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"

#include <cstdint>

namespace rl4 {

struct RigMetrics {
    using Pointer = UniqueInstance<RigMetrics>::PointerType;

    std::uint16_t lodCount;
    std::uint16_t guiControlCount;
    std::uint16_t rawControlCount;
    std::uint16_t psdControlCount;
    std::uint16_t mlControlCount;
    std::uint16_t jointAttributeCount;
    std::uint16_t blendShapeCount;
    std::uint16_t animatedMapCount;
    std::uint16_t neuralNetworkCount;

    explicit RigMetrics(MemoryResource*  /*unused*/) :
        lodCount{},
        guiControlCount{},
        rawControlCount{},
        psdControlCount{},
        mlControlCount{},
        jointAttributeCount{},
        blendShapeCount{},
        animatedMapCount{},
        neuralNetworkCount{} {
    }

    template<class Archive>
    void serialize(Archive& archive) {
        archive(lodCount,
                guiControlCount,
                rawControlCount,
                psdControlCount,
                mlControlCount,
                jointAttributeCount,
                blendShapeCount,
                animatedMapCount,
                neuralNetworkCount);
    }

};

}  // namespace rl4
