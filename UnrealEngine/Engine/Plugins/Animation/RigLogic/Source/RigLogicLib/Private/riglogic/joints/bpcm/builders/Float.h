// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/joints/bpcm/JointsBuilderCommon.h"
#include "riglogic/types/Aliases.h"

#include <cstddef>

namespace rl4 {

namespace bpcm {

class FloatStorageBuilder : public JointsBuilderCommon<float> {
    public:
        FloatStorageBuilder(std::uint32_t blockHeight_, std::uint32_t padTo_, MemoryResource* memRes_);
        ~FloatStorageBuilder();

    protected:
        void setValues(const dna::BehaviorReader* reader) override;

};

}  // namespace bpcm

}  // namespace rl4
