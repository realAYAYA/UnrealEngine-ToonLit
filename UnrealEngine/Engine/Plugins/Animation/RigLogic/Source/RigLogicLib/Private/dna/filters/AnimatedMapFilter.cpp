// Copyright Epic Games, Inc. All Rights Reserved.

#include "dna/filters/AnimatedMapFilter.h"

#include "dna/DNA.h"
#include "dna/TypeDefs.h"
#include "dna/filters/Remap.h"
#include "dna/utils/Extd.h"

namespace dna {

AnimatedMapFilter::AnimatedMapFilter(MemoryResource* memRes_) :
    memRes{memRes_},
    passingIndices{memRes},
    remappedIndices{memRes} {
}

void AnimatedMapFilter::apply(RawDefinition& dest) {
    // Collect all distinct element position indices that are referenced by the present LODs
    dest.lodAnimatedMapMapping.mergeIndicesInto(passingIndices);
    // Fill the structure that maps indices prior to deletion to indices after deletion
    remap(static_cast<std::uint16_t>(dest.animatedMapNames.size()), passingIndices, remappedIndices);
    // Fix indices so they match the same elements as earlier (but their
    // actual position changed with the deletion of the unneeded entries)
    dest.lodAnimatedMapMapping.mapIndices([this](std::uint16_t value) {
            return remappedIndices.at(value);
        });
    // Delete elements that are not referenced by the new subset of LODs
    extd::filter(dest.animatedMapNames, extd::byPosition(passingIndices));
}

bool AnimatedMapFilter::passes(std::uint16_t index) const {
    return extd::contains(passingIndices, index);
}

}  // namespace dna
