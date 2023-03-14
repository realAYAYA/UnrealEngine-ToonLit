// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/JointsFactory.h"

#include "riglogic/joints/JointsBuilder.h"
#include "riglogic/joints/JointsEvaluator.h"
#include "riglogic/riglogic/Configuration.h"
#include "riglogic/transformation/Transformation.h"
#include "riglogic/types/Aliases.h"

#include <dna/layers/BehaviorReader.h>
#include <pma/utils/ManagedInstance.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rl4 {

namespace {

constexpr std::size_t txOffset = 0ul;
constexpr std::size_t tyOffset = 1ul;
constexpr std::size_t tzOffset = 2ul;
constexpr std::size_t rxOffset = 3ul;
constexpr std::size_t ryOffset = 4ul;
constexpr std::size_t rzOffset = 5ul;

}  // namespace

template<class TSource, class TDestination>
static void scatter(const TSource& source, TDestination& destination, std::size_t stride, std::size_t offset) {
    const auto size = static_cast<std::size_t>(std::distance(std::begin(source), std::end(source)));
    for (std::size_t i = 0ul; i < size; ++i) {
        destination[i * stride + offset] = source[i];
    }
}

static Vector<float> copyNeutralValues(const dna::BehaviorReader* reader, MemoryResource* memRes) {
    Vector<float> neutralValues{memRes};
    neutralValues.resize(reader->getJointCount() * Transformation::size());
    std::fill(neutralValues.begin(), neutralValues.end(), 1.0f);
    scatter(reader->getNeutralJointTranslationXs(), neutralValues, Transformation::size(), txOffset);
    scatter(reader->getNeutralJointTranslationYs(), neutralValues, Transformation::size(), tyOffset);
    scatter(reader->getNeutralJointTranslationZs(), neutralValues, Transformation::size(), tzOffset);
    scatter(reader->getNeutralJointRotationXs(), neutralValues, Transformation::size(), rxOffset);
    scatter(reader->getNeutralJointRotationYs(), neutralValues, Transformation::size(), ryOffset);
    scatter(reader->getNeutralJointRotationZs(), neutralValues, Transformation::size(), rzOffset);
    return neutralValues;
}

static Matrix<std::uint16_t> copyVariableAttributeIndices(const dna::BehaviorReader* reader, MemoryResource* memRes) {
    Matrix<std::uint16_t> variableAttributeIndices{memRes};
    variableAttributeIndices.resize(reader->getLODCount());
    for (std::uint16_t lod = 0u; lod < reader->getLODCount(); ++lod) {
        auto indices = reader->getJointVariableAttributeIndices(lod);
        variableAttributeIndices[lod].assign(indices.begin(), indices.end());
    }
    return variableAttributeIndices;
}

JointsFactory::JointsPtr JointsFactory::create(const Configuration& config,
                                               const dna::BehaviorReader* reader,
                                               MemoryResource* memRes) {
    auto neutralValues = copyNeutralValues(reader, memRes);
    auto variableAttributeIndices = copyVariableAttributeIndices(reader, memRes);
    auto builder = JointsBuilder::create(config, memRes);
    builder->allocateStorage(reader);
    builder->fillStorage(reader);
    auto evaluator = builder->build();
    return pma::UniqueInstance<Joints>::with(memRes).create(std::move(evaluator),
                                                            std::move(neutralValues),
                                                            std::move(variableAttributeIndices),
                                                            reader->getJointGroupCount());
}

JointsFactory::JointsPtr JointsFactory::create(const Configuration& config, const RigMetrics&  /*unused*/,
                                               MemoryResource* memRes) {
    auto builder = JointsBuilder::create(config, memRes);
    auto evaluator = builder->build();
    return pma::UniqueInstance<Joints>::with(memRes).create(std::move(evaluator), memRes);
}

}  // namespace rl4
