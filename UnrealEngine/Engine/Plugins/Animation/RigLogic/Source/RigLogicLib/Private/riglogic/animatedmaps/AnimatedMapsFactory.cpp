// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/animatedmaps/AnimatedMapsFactory.h"

#include "riglogic/conditionaltable/ConditionalTable.h"
#include "riglogic/utils/Extd.h"

#include <dna/layers/BehaviorReader.h>

#include <cstddef>

namespace rl4 {

AnimatedMapsFactory::AnimatedMapsPtr AnimatedMapsFactory::create(MemoryResource* memRes) {
    return ManagedAnimatedMaps::with(memRes).create(Vector<std::uint16_t>{memRes},
                                                    ConditionalTable{memRes});
}

AnimatedMapsFactory::AnimatedMapsPtr AnimatedMapsFactory::create(const dna::BehaviorReader* reader, MemoryResource* memRes) {
    Vector<std::uint16_t> lods{memRes};
    Vector<std::uint16_t> inputIndices{memRes};
    Vector<std::uint16_t> outputIndices{memRes};
    Vector<float> fromValues{memRes};
    Vector<float> toValues{memRes};
    Vector<float> slopeValues{memRes};
    Vector<float> cutValues{memRes};

    extd::copy(reader->getAnimatedMapLODs(), lods);
    extd::copy(reader->getAnimatedMapInputIndices(), inputIndices);
    extd::copy(reader->getAnimatedMapOutputIndices(), outputIndices);
    extd::copy(reader->getAnimatedMapFromValues(), fromValues);
    extd::copy(reader->getAnimatedMapToValues(), toValues);
    extd::copy(reader->getAnimatedMapSlopeValues(), slopeValues);
    extd::copy(reader->getAnimatedMapCutValues(), cutValues);
    // DNAs may contain these parameters in reverse order
    // i.e. the `from` value is actually larger than the `to` value
    assert(fromValues.size() == toValues.size());
    for (std::size_t i = 0ul; i < fromValues.size(); ++i) {
        if (fromValues[i] > toValues[i]) {
            std::swap(fromValues[i], toValues[i]);
        }
    }

    const auto inputCount = static_cast<std::uint16_t>(reader->getRawControlCount() + reader->getPSDCount());
    const auto outputCount = reader->getAnimatedMapCount();
    ConditionalTable conditionals{std::move(inputIndices),
                                  std::move(outputIndices),
                                  std::move(fromValues),
                                  std::move(toValues),
                                  std::move(slopeValues),
                                  std::move(cutValues),
                                  inputCount,
                                  outputCount};
    return ManagedAnimatedMaps::with(memRes).create(std::move(lods), std::move(conditionals));
}

}  // namespace rl4
