// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "dna/TypeDefs.h"

#include <cstdint>

namespace dna {

struct RawDefinition;

class JointFilter {
    public:
        explicit JointFilter(MemoryResource* memRes_);
        void apply(RawDefinition& dest);
        bool passes(std::uint16_t index) const;
        std::uint16_t remapped(std::uint16_t oldIndex) const;
        std::uint16_t maxRemappedIndex() const;

    private:
        MemoryResource* memRes;
        UnorderedSet<std::uint16_t> passingIndices;
        UnorderedMap<std::uint16_t, std::uint16_t> remappedIndices;

};

}  // namespace dna
