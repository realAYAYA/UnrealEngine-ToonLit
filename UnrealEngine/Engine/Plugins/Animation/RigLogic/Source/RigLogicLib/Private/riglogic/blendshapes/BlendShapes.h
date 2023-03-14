// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/types/Aliases.h"

#include <cstddef>

namespace rl4 {

class BlendShapes {
    public:
        BlendShapes(Vector<std::uint16_t>&& lods_, Vector<std::uint16_t>&& inputIndices_, Vector<std::uint16_t>&& outputIndices_);

        void calculate(ConstArrayView<float> inputs, ArrayView<float> outputs, std::uint16_t lod) const;

        template<class Archive>
        void serialize(Archive& archive) {
            archive(lods, inputIndices, outputIndices);
        }

    private:
        Vector<std::uint16_t> lods;
        Vector<std::uint16_t> inputIndices;
        Vector<std::uint16_t> outputIndices;

};

}  // namespace rl4
