// Copyright Epic Games, Inc. All Rights Reserved.

#include "rltests/Defs.h"
#include "rltests/blendshapes/BlendShapeFixtures.h"

#include "riglogic/blendshapes/BlendShapes.h"
#include "riglogic/types/Aliases.h"
#include "riglogic/utils/Extd.h"

#include <pma/resources/AlignedMemoryResource.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <array>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace {

class BlendShapesTest : public ::testing::TestWithParam<std::uint16_t> {
};

}  // namespace

TEST_P(BlendShapesTest, Calculate) {
    using namespace rltests;

    // Test input parameter - Which LOD level to test
    const auto lod = GetParam();

    pma::AlignedMemoryResource amr;
    auto blendShapes = createTestBlendShapes(&amr);

    std::vector<float> outputs(blendShapeOutputs.size());
    blendShapes.calculate(rl4::ConstArrayView<float>{blendShapeInputs},
                          rl4::ArrayView<float>{outputs},
                          lod);

    std::vector<float> expectedData{blendShapeOutputs.begin(), blendShapeOutputs.end()};
    std::fill(extd::advanced(expectedData.begin(), LODs[lod]), expectedData.end(), 0.0f);
    rl4::ConstArrayView<float> expected{expectedData};

    for (std::size_t i = 0ul; i < blendShapeOutputs.size(); ++i) {
        ASSERT_EQ(outputs[i], expected[i]);
    }
}

INSTANTIATE_TEST_SUITE_P(BlendShapesTest, BlendShapesTest, ::testing::Values(0u, 1u, 2u, 3u));
