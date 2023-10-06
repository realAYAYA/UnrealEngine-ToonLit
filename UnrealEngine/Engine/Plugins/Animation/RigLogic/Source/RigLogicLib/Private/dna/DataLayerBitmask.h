// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "dna/DataLayer.h"
#include "dna/utils/ScopedEnumEx.h"

namespace dna {

enum class DataLayerBitmask {
    Descriptor = 1,
    Definition = 2,
    Behavior = 4,
    GeometryBlendShapesOnly = 8,
    GeometryRest = 16,
    MachineLearnedBehavior = 32
};

inline DataLayerBitmask computeDataLayerBitmask(DataLayer layer) {
    DataLayerBitmask result = DataLayerBitmask::Descriptor;
    if (layer == DataLayer::All) {
        result |= DataLayerBitmask::Definition;
        result |= DataLayerBitmask::Behavior;
        result |= DataLayerBitmask::GeometryBlendShapesOnly;
        result |= DataLayerBitmask::GeometryRest;
        result |= DataLayerBitmask::MachineLearnedBehavior;
        return result;
    }

    if (contains(layer, DataLayer::Definition)) {
        result |= DataLayerBitmask::Definition;
    }

    if (contains(layer, DataLayer::Behavior)) {
        result |= DataLayerBitmask::Behavior;
    }

    if (contains(layer, DataLayer::Geometry)) {
        result |= DataLayerBitmask::GeometryBlendShapesOnly;
        result |= DataLayerBitmask::GeometryRest;
    }

    if (contains(layer, DataLayer::GeometryWithoutBlendShapes)) {
        result |= DataLayerBitmask::GeometryRest;
    }

    if (contains(layer, DataLayer::MachineLearnedBehavior)) {
        result |= DataLayerBitmask::MachineLearnedBehavior;
    }

    return result;
}

}  // namespace dna
