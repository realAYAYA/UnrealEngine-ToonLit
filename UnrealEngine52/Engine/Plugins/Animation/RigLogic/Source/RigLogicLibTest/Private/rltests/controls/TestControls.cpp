// Copyright Epic Games, Inc. All Rights Reserved.

#include "rltests/Defs.h"
#include "rltests/conditionaltable/ConditionalTableFixtures.h"
#include "rltests/controls/ControlFixtures.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/controls/Controls.h"

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
    const std::uint16_t guiControlCount = conditionals.getInputCount();
    const std::uint16_t rawControlCount = conditionals.getOutputCount();
    const std::uint16_t psdControlCount = psds.getDistinctPSDCount();
    auto instanceFactory = ControlsFactory::getInstanceFactory(guiControlCount, rawControlCount, psdControlCount, 0u);
    rl4::Controls controls{std::move(conditionals), std::move(psds), instanceFactory};

    const rl4::Vector<float> guiControls{0.1f, 0.2f};
    const rl4::Vector<float> expected{0.3f, 0.6f};

    auto instance = controls.createInstance(&amr);
    auto guiBuffer = instance->getGUIControlBuffer();
    auto rawBuffer = instance->getInputBuffer();
    std::copy(guiControls.begin(), guiControls.end(), guiBuffer.begin());
    controls.mapGUIToRaw(instance.get());

    ASSERT_EQ(rawBuffer.size(), expected.size());
    ASSERT_ELEMENTS_EQ(rawBuffer, expected, expected.size());
}

TEST(ControlsTest, PSDsAppendToOutput) {
    pma::AlignedMemoryResource amr;
    auto conditionals = ConditionalTableFactory::withMultipleIODefaults(&amr);

    rl4::Vector<float> rawControls{0.1f, 0.2f};
    const rl4::Vector<float> expected{0.1f, 0.2f, 0.4f, 0.6f};

    rl4::Vector<std::uint16_t> rows{2u, 3u};
    rl4::Vector<std::uint16_t> cols{0u, 1u};
    rl4::Vector<float> values{4.0f, 3.0f};
    rl4::PSDMatrix psds{2u, std::move(rows), std::move(cols), std::move(values)};

    const std::uint16_t guiControlCount = conditionals.getInputCount();
    const std::uint16_t rawControlCount = conditionals.getOutputCount();
    const std::uint16_t psdControlCount = psds.getDistinctPSDCount();
    auto instanceFactory = ControlsFactory::getInstanceFactory(guiControlCount, rawControlCount, psdControlCount, 0u);
    rl4::Controls controls{std::move(conditionals), std::move(psds), instanceFactory};
    auto instance = controls.createInstance(&amr);
    auto buffer = instance->getInputBuffer();
    std::copy(rawControls.begin(), rawControls.end(), buffer.begin());

    controls.calculate(instance.get());

    ASSERT_EQ(buffer.size(), expected.size());
    ASSERT_ELEMENTS_EQ(buffer, expected, expected.size());
}
