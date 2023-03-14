// Copyright Epic Games, Inc. All Rights Reserved.

#include "rltests/Defs.h"
#include "rltests/conditionaltable/ConditionalTableFixtures.h"

#include "riglogic/controls/Controls.h"
#include "riglogic/types/Aliases.h"

#include <pma/resources/AlignedMemoryResource.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <array>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

TEST(ControlsTest, GUIToRawMapping) {
    pma::AlignedMemoryResource amr;
    auto conditionals = ConditionalTableFactory::withMultipleIODefaults(&amr);

    rl4::PSDMatrix psds{0u, {}, {}, {}};
    rl4::Controls controls{std::move(conditionals), std::move(psds)};

    const rl4::Vector<float> guiControls{0.1f, 0.2f};
    const rl4::Vector<float> expected{0.3f, 0.6f};

    std::array<float, 2ul> outputs;
    controls.mapGUIToRaw(rl4::ConstArrayView<float>{guiControls}, rl4::ArrayView<float>{outputs});

    ASSERT_ELEMENTS_EQ(outputs, expected, 2ul);
}

TEST(ControlsTest, PSDsAppendToOutput) {
    pma::AlignedMemoryResource amr;
    auto conditionals = ConditionalTableFactory::withMultipleIODefaults(&amr);

    rl4::Vector<std::uint16_t> rows{2u, 3u};
    rl4::Vector<std::uint16_t> cols{0u, 1u};
    rl4::Vector<float> values{4.0f, 3.0f};
    rl4::PSDMatrix psds{2u, std::move(rows), std::move(cols), std::move(values)};

    rl4::Controls controls{std::move(conditionals), std::move(psds)};

    rl4::Vector<float> rawControls{0.1f, 0.2f, 0.0f, 0.0f};
    const rl4::Vector<float> expected{0.1f, 0.2f, 0.4f, 0.6f};
    controls.calculate(rl4::ArrayView<float>{rawControls});

    ASSERT_ELEMENTS_EQ(rawControls, expected, 4ul);
}
