// Copyright Epic Games, Inc. All Rights Reserved.

#include "rltests/Defs.h"

#include "riglogic/transformation/Transformation.h"

#include <cassert>

TEST(TransformationTest, AccessByIndex) {
    std::vector<float> vec{0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
                           9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f};

    assert(vec.size() % 9ul == 0ul);

    rl4::TransformationArrayView transformationView = {vec.data(), vec.size()};

    auto translation0 = transformationView[0].getTranslation();
    ASSERT_FLOAT_EQ(translation0.x, 0.0f);
    ASSERT_FLOAT_EQ(translation0.y, 1.0f);
    ASSERT_FLOAT_EQ(translation0.z, 2.0f);

    auto rotation0 = transformationView[0].getRotation();
    ASSERT_FLOAT_EQ(rotation0.x, 3.0f);
    ASSERT_FLOAT_EQ(rotation0.y, 4.0f);
    ASSERT_FLOAT_EQ(rotation0.z, 5.0f);

    auto scale0 = transformationView[0].getScale();
    ASSERT_FLOAT_EQ(scale0.x, 6.0f);
    ASSERT_FLOAT_EQ(scale0.y, 7.0f);
    ASSERT_FLOAT_EQ(scale0.z, 8.0f);

    auto translation1 = transformationView[1].getTranslation();
    ASSERT_FLOAT_EQ(translation1.x, 9.0f);
    ASSERT_FLOAT_EQ(translation1.y, 10.0f);
    ASSERT_FLOAT_EQ(translation1.z, 11.0f);

    auto rotation1 = transformationView[1].getRotation();
    ASSERT_FLOAT_EQ(rotation1.x, 12.0f);
    ASSERT_FLOAT_EQ(rotation1.y, 13.0f);
    ASSERT_FLOAT_EQ(rotation1.z, 14.0f);

    auto scale1 = transformationView[1].getScale();
    ASSERT_FLOAT_EQ(scale1.x, 15.0f);
    ASSERT_FLOAT_EQ(scale1.y, 16.0f);
    ASSERT_FLOAT_EQ(scale1.z, 17.0f);
}

TEST(TransformationTest, TransformationIterator) {
    std::vector<float> vec{0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
                           9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f};

    assert(vec.size() % 9ul == 0ul);

    rl4::TransformationArrayView transformationView = {vec.data(), vec.size()};

    std::size_t index = 0ul;
    for (auto transform : transformationView) {
        auto translation = transform.getTranslation();
        ASSERT_FLOAT_EQ(translation.x, vec[index]);
        ASSERT_FLOAT_EQ(translation.y, vec[index + 1ul]);
        ASSERT_FLOAT_EQ(translation.z, vec[index + 2ul]);

        auto rotation = transform.getRotation();
        ASSERT_FLOAT_EQ(rotation.x, vec[index + 3ul]);
        ASSERT_FLOAT_EQ(rotation.y, vec[index + 4ul]);
        ASSERT_FLOAT_EQ(rotation.z, vec[index + 5ul]);

        auto scale = transform.getScale();
        ASSERT_FLOAT_EQ(scale.x, vec[index + 6ul]);
        ASSERT_FLOAT_EQ(scale.y, vec[index + 7ul]);
        ASSERT_FLOAT_EQ(scale.z, vec[index + 8ul]);
        index += 9ul;
    }
}
