// Copyright Epic Games, Inc. All Rights Reserved.

#include "dna/filters/JointFilter.h"

#include "dna/DNA.h"
#include "dna/TypeDefs.h"
#include "dna/filters/Remap.h"
#include "dna/utils/Extd.h"

namespace dna {

JointFilter::JointFilter(MemoryResource* memRes_) :
    memRes{memRes_},
    passingIndices{memRes},
    remappedIndices{memRes} {
}

void JointFilter::apply(RawDefinition& dest) {
    // Collect all distinct element position indices that are referenced by the present LODs
    dest.lodJointMapping.mergeIndicesInto(passingIndices);
    // Fill the structure that maps indices prior to deletion to indices after deletion
    remap(static_cast<std::uint16_t>(dest.jointNames.size()), passingIndices, remappedIndices);
    // Fix indices so they match the same elements as earlier (but their
    // actual position changed with the deletion of the unneeded entries)
    dest.lodJointMapping.mapIndices([this](std::uint16_t value) {
            return remappedIndices.at(value);
        });
    // Delete elements that are not referenced by the new subset of LODs
    extd::filter(dest.jointNames, extd::byPosition(passingIndices));
    extd::filter(dest.jointHierarchy, extd::byPosition(passingIndices));
    // Fix joint hierarchy indices
    for (auto& jntIdx : dest.jointHierarchy) {
        jntIdx = remappedIndices[jntIdx];
    }
    // Delete entries from other mappings that reference any of the deleted elements
    extd::filter(dest.neutralJointTranslations.xs, extd::byPosition(passingIndices));
    extd::filter(dest.neutralJointTranslations.ys, extd::byPosition(passingIndices));
    extd::filter(dest.neutralJointTranslations.zs, extd::byPosition(passingIndices));
    extd::filter(dest.neutralJointRotations.xs, extd::byPosition(passingIndices));
    extd::filter(dest.neutralJointRotations.ys, extd::byPosition(passingIndices));
    extd::filter(dest.neutralJointRotations.zs, extd::byPosition(passingIndices));
}

bool JointFilter::passes(std::uint16_t index) const {
    return extd::contains(passingIndices, index);
}

std::uint16_t JointFilter::remapped(std::uint16_t oldIndex) const {
    return remappedIndices.at(oldIndex);
}

std::uint16_t JointFilter::maxRemappedIndex() const {
    return (remappedIndices.empty() ? static_cast<std::uint16_t>(0) : extd::maxOf(remappedIndices).second);
}

}  // namespace dna
