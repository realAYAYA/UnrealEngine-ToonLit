// Copyright Epic Games, Inc. All Rights Reserved.

#include "rltests/Defs.h"

#include "riglogic/psdmatrix/PSDMatrix.h"

#include <pma/resources/AlignedMemoryResource.h>
#include <pma/utils/ManagedInstance.h>

TEST(PSDMatrixTest, OutputsAreClamped) {
    pma::AlignedMemoryResource amr;

    rl4::Vector<std::uint16_t> rows{1u};
    rl4::Vector<std::uint16_t> cols{0u};
    rl4::Vector<float> values{100.0f};
    rl4::PSDMatrix psds{1ul, std::move(rows), std::move(cols), std::move(values)};

    float inputs[] = {0.1f, 0.0f};
    const float expected[] = {1.0f};

    psds.calculate(inputs, 1ul);
    ASSERT_ELEMENTS_EQ((inputs + 1ul), expected, 1);
}

TEST(PSDMatrixTest, OutputsKeepExistingProduct) {
    pma::AlignedMemoryResource amr;

    rl4::Vector<std::uint16_t> rows{2u, 2u};
    rl4::Vector<std::uint16_t> cols{0u, 1u};
    rl4::Vector<float> values{4.0f, 10.0f};
    rl4::PSDMatrix psds{1ul, std::move(rows), std::move(cols), std::move(values)};

    float inputs[] = {0.1f, 0.2f, 0.0f};
    const float expected[] = {0.8f};

    psds.calculate(inputs, 2ul);
    ASSERT_ELEMENTS_EQ((inputs + 2ul), expected, 1);
}

TEST(PSDMatrixTest, RowsSpecifyDestinationIndex) {
    pma::AlignedMemoryResource amr;

    rl4::Vector<std::uint16_t> rows{2u, 3u};
    rl4::Vector<std::uint16_t> cols{0u, 1u};
    rl4::Vector<float> values{4.0f, 3.0f};
    rl4::PSDMatrix psds{2ul, std::move(rows), std::move(cols), std::move(values)};

    float inputs[] = {0.1f, 0.2f, 0.0f, 0.0f};
    const float expected[] = {0.4f, 0.6f};

    psds.calculate(inputs, 2ul);
    ASSERT_ELEMENTS_EQ((inputs + 2ul), expected, 2);
}
