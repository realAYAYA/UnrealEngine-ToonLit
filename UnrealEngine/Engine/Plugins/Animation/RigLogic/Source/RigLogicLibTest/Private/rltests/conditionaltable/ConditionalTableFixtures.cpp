// Copyright Epic Games, Inc. All Rights Reserved.

#include "rltests/conditionaltable/ConditionalTableFixtures.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/conditionaltable/ConditionalTable.h"

namespace single {

const std::uint16_t inputIndices[] = {0, 0};
const std::uint16_t outputIndices[] = {0, 0};
const float fromValues[] = {0.0f, 0.0f};
const float toValues[] = {1.0f, 1.0f};
const float slopeValues[] = {1.0f, 1.0f};
const float cutValues[] = {0.1f, 0.3f};
const std::uint16_t size = 2ul;
const std::uint16_t distinctInputs = 1ul;
const std::uint16_t distinctOutputs = 1ul;

}  // namespace single

namespace multiple {

const std::uint16_t inputIndices[] = {0, 1};
const std::uint16_t outputIndices[] = {0, 1};
const float fromValues[] = {0.0f, 0.0f};
const float toValues[] = {1.0f, 1.0f};
const float slopeValues[] = {1.0f, 1.0f};
const float cutValues[] = {0.2f, 0.4f};
const std::uint16_t size = 2ul;
const std::uint16_t distinctInputs = 2ul;
const std::uint16_t distinctOutputs = 2ul;

}  // namespace multiple

rl4::ConditionalTable ConditionalTableFactory::withSingleIO(rl4::Vector<float>&& fromValues,
                                                            rl4::Vector<float>&& toValues,
                                                            rl4::MemoryResource* memRes) {
    return rl4::ConditionalTable{
        rl4::Vector<std::uint16_t>{single::inputIndices, single::inputIndices + single::size, memRes},
        rl4::Vector<std::uint16_t>{single::outputIndices, single::outputIndices + single::size, memRes},
        std::move(fromValues),
        std::move(toValues),
        rl4::Vector<float>{single::slopeValues, single::slopeValues + single::size, memRes},
        rl4::Vector<float>{single::cutValues, single::cutValues + single::size, memRes},
        single::distinctInputs,
        single::distinctOutputs
    };
}

rl4::ConditionalTable ConditionalTableFactory::withMultipleIO(rl4::Vector<float>&& fromValues,
                                                              rl4::Vector<float>&& toValues,
                                                              rl4::MemoryResource* memRes) {
    return rl4::ConditionalTable{
        rl4::Vector<std::uint16_t>{multiple::inputIndices, multiple::inputIndices + multiple::size, memRes},
        rl4::Vector<std::uint16_t>{multiple::outputIndices, multiple::outputIndices + multiple::size, memRes},
        std::move(fromValues),
        std::move(toValues),
        rl4::Vector<float>{multiple::slopeValues, multiple::slopeValues + multiple::size, memRes},
        rl4::Vector<float>{multiple::cutValues, multiple::cutValues + multiple::size, memRes},
        multiple::distinctInputs,
        multiple::distinctOutputs
    };
}

rl4::ConditionalTable ConditionalTableFactory::withSingleIODefaults(rl4::MemoryResource* memRes) {
    return withSingleIO(
        rl4::Vector<float>{single::fromValues, single::fromValues + single::size, memRes},
        rl4::Vector<float>{single::toValues, single::toValues + single::size, memRes},
        memRes
        );
}

rl4::ConditionalTable ConditionalTableFactory::withMultipleIODefaults(rl4::MemoryResource* memRes) {
    return withMultipleIO(
        rl4::Vector<float>{multiple::fromValues, multiple::fromValues + multiple::size, memRes},
        rl4::Vector<float>{multiple::toValues, multiple::toValues + multiple::size, memRes},
        memRes
        );
}
