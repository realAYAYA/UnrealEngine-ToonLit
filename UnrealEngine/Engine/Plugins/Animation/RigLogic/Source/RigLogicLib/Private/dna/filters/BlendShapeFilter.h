// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "dna/TypeDefs.h"

#include <cstdint>

namespace dna {

struct RawDefinition;

class BlendShapeFilter {
    public:
        explicit BlendShapeFilter(MemoryResource* memRes_);
        void apply(RawDefinition& dest);
        bool passes(std::uint16_t index) const;

    private:
        MemoryResource* memRes;
        UnorderedSet<std::uint16_t> passingIndices;
        UnorderedMap<std::uint16_t, std::uint16_t> remappedIndices;

};

}  // namespace dna
