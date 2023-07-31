// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/conditionaltable/ConditionalTable.h"
#include "riglogic/types/Aliases.h"

#include <cstddef>

namespace rl4 {

class AnimatedMaps {
    public:
        AnimatedMaps(Vector<std::uint16_t>&& lods_, ConditionalTable&& conditionals_);

        void calculate(ConstArrayView<float> inputs, ArrayView<float> outputs, std::uint16_t lod) const;

        template<class Archive>
        void serialize(Archive& archive) {
            archive(lods, conditionals);
        }

    private:
        Vector<std::uint16_t> lods;
        ConditionalTable conditionals;

};

}  // namespace rl4
