// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <cstdint>

namespace rl4 {

struct RigMetrics {
    std::uint16_t lodCount;
    std::uint16_t guiControlCount;
    std::uint16_t rawControlCount;
    std::uint16_t psdCount;
    std::uint16_t jointAttributeCount;
    std::uint16_t blendShapeCount;
    std::uint16_t animatedMapCount;

    template<class Archive>
    void serialize(Archive& archive) {
        archive(lodCount,
                guiControlCount,
                rawControlCount,
                psdCount,
                jointAttributeCount,
                blendShapeCount,
                animatedMapCount);
    }

};

}  // namespace rl4
