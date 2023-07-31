// Copyright Epic Games, Inc. All Rights Reserved.

#include "dnatests/Defs.h"
#include "dnatests/Fixtures.h"

#include "dna/DataLayer.h"
#include "dna/DNA.h"
#include "dna/stream/FilteredInputArchive.h"

#include <pma/resources/AlignedMemoryResource.h>

#include <memory>

namespace {

struct LODConstraint {
    std::uint16_t maxLOD;
    std::uint16_t minLOD;
};

class FilteredDNAInputArchiveTest : public ::testing::TestWithParam<LODConstraint> {
    protected:
        void SetUp() override {
            dnaInstance.reset(new dna::DNA{&amr});
            lodConstraint = GetParam();

            const auto bytes = dna::raw::getBytes();
            auto stream = pma::makeScoped<trio::MemoryStream>();
            stream->write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            stream->seek(0);

            dna::FilteredInputArchive archive{stream.get(), dna::DataLayer::All, lodConstraint.maxLOD, lodConstraint.minLOD,
                                              & amr};
            archive >> *dnaInstance;
        }

    protected:
        pma::AlignedMemoryResource amr;
        std::unique_ptr<dna::DNA> dnaInstance;
        LODConstraint lodConstraint;
};

}  // namespace

TEST_P(FilteredDNAInputArchiveTest, FilterJoints) {
    const auto& result = dnaInstance->behavior.joints;
    const auto expected = dna::decoded::Fixtures::getJoints(lodConstraint.maxLOD, lodConstraint.minLOD, &amr);

    ASSERT_EQ(result.rowCount, expected.rowCount);
    ASSERT_EQ(result.colCount, expected.colCount);
    ASSERT_EQ(result.jointGroups.size(), expected.jointGroups.size());
    for (std::size_t jointGroupIdx = 0ul; jointGroupIdx < expected.jointGroups.size(); ++jointGroupIdx) {
        const auto& jntGrp = result.jointGroups[jointGroupIdx];
        const auto& expectedJntGrp = expected.jointGroups[jointGroupIdx];
        ASSERT_ELEMENTS_EQ(jntGrp.lods, expectedJntGrp.lods, expectedJntGrp.lods.size());
        ASSERT_ELEMENTS_EQ(jntGrp.inputIndices, expectedJntGrp.inputIndices, expectedJntGrp.inputIndices.size());
        ASSERT_ELEMENTS_EQ(jntGrp.outputIndices, expectedJntGrp.outputIndices, expectedJntGrp.outputIndices.size());
        ASSERT_ELEMENTS_NEAR(jntGrp.values, expectedJntGrp.values, expectedJntGrp.values.size(), 0.005f);
        ASSERT_ELEMENTS_EQ(jntGrp.jointIndices, expectedJntGrp.jointIndices, expectedJntGrp.jointIndices.size());
    }
}

TEST_P(FilteredDNAInputArchiveTest, FilterBlendShapes) {
    const auto& result = dnaInstance->behavior.blendShapeChannels;
    const auto expected = dna::decoded::Fixtures::getBlendShapes(lodConstraint.maxLOD, lodConstraint.minLOD, &amr);

    ASSERT_ELEMENTS_EQ(result.inputIndices, expected.inputIndices, expected.inputIndices.size());
    ASSERT_ELEMENTS_EQ(result.outputIndices, expected.outputIndices, expected.outputIndices.size());
    ASSERT_ELEMENTS_EQ(result.lods, expected.lods, expected.lods.size());
}

TEST_P(FilteredDNAInputArchiveTest, FilterAnimatedMaps) {
    const auto& result = dnaInstance->behavior.animatedMaps;
    const auto expected = dna::decoded::Fixtures::getAnimatedMaps(lodConstraint.maxLOD, lodConstraint.minLOD, &amr);

    ASSERT_ELEMENTS_EQ(result.lods, expected.lods, expected.lods.size());
    ASSERT_ELEMENTS_EQ(result.conditionals.inputIndices,
                       expected.conditionals.inputIndices,
                       expected.conditionals.inputIndices.size());
    ASSERT_ELEMENTS_EQ(result.conditionals.outputIndices,
                       expected.conditionals.outputIndices,
                       expected.conditionals.outputIndices.size());
    ASSERT_ELEMENTS_NEAR(result.conditionals.fromValues,
                         expected.conditionals.fromValues,
                         expected.conditionals.fromValues.size(),
                         0.005f);
    ASSERT_ELEMENTS_NEAR(result.conditionals.toValues,
                         expected.conditionals.toValues,
                         expected.conditionals.toValues.size(),
                         0.005f);
    ASSERT_ELEMENTS_NEAR(result.conditionals.slopeValues,
                         expected.conditionals.slopeValues,
                         expected.conditionals.slopeValues.size(),
                         0.005f);
    ASSERT_ELEMENTS_NEAR(result.conditionals.cutValues,
                         expected.conditionals.cutValues,
                         expected.conditionals.cutValues.size(),
                         0.005f);
}

TEST_P(FilteredDNAInputArchiveTest, FilterDefinition) {
    const auto index = dna::decoded::Fixtures::lodConstraintToIndex(lodConstraint.maxLOD, lodConstraint.minLOD);
    ASSERT_EQ(dnaInstance->descriptor.lodCount, dna::decoded::lodCount[index]);
    const auto& result = dnaInstance->definition;

    ASSERT_ELEMENTS_EQ(result.jointHierarchy, dna::decoded::jointHierarchy[index], dna::decoded::jointHierarchy[index].size());

    for (std::uint16_t lod = 0u; lod < dnaInstance->descriptor.lodCount; ++lod) {
        auto jointIndices = result.lodJointMapping.getIndices(lod);
        ASSERT_EQ(jointIndices.size(), dna::decoded::jointNames[index][lod].size());
        for (std::size_t i = 0ul; i < jointIndices.size(); ++i) {
            ASSERT_EQ(result.jointNames[jointIndices[i]], dna::decoded::jointNames[index][lod][i]);
        }

        auto blendShapeIndices = result.lodBlendShapeMapping.getIndices(lod);
        ASSERT_EQ(blendShapeIndices.size(), dna::decoded::blendShapeNames[index][lod].size());
        for (std::size_t i = 0ul; i < blendShapeIndices.size(); ++i) {
            ASSERT_EQ(result.blendShapeChannelNames[blendShapeIndices[i]], dna::decoded::blendShapeNames[index][lod][i]);
        }

        auto animatedMapIndices = result.lodAnimatedMapMapping.getIndices(lod);
        ASSERT_EQ(animatedMapIndices.size(), dna::decoded::animatedMapNames[index][lod].size());
        for (std::size_t i = 0ul; i < animatedMapIndices.size(); ++i) {
            ASSERT_EQ(result.animatedMapNames[animatedMapIndices[i]], dna::decoded::animatedMapNames[index][lod][i]);
        }

        auto meshIndices = result.lodMeshMapping.getIndices(lod);
        ASSERT_EQ(meshIndices.size(), dna::decoded::meshNames[index][lod].size());
        for (std::size_t i = 0ul; i < meshIndices.size(); ++i) {
            ASSERT_EQ(result.meshNames[meshIndices[i]], dna::decoded::meshNames[index][lod][i]);
        }
    }

    ASSERT_EQ(result.meshNames.size(), dna::decoded::meshCount[index]);
}

INSTANTIATE_TEST_SUITE_P(FilteredDNAInputArchiveTest, FilteredDNAInputArchiveTest, ::testing::Values(
                             LODConstraint{0u, 1u},
                             LODConstraint{1u, 1u},
                             LODConstraint{0u, 0u}
                             ));

namespace {

class GeometryFilteringTest : public ::testing::Test {
    protected:
        void SetUp() override {
            dnaInstance.reset(new dna::DNA{&amr});

            const auto bytes = dna::raw::getBytes();
            stream = pma::makeScoped<trio::MemoryStream>();
            stream->write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            stream->seek(0);
        }

    protected:
        pma::AlignedMemoryResource amr;
        pma::ScopedPtr<trio::MemoryStream, pma::FactoryDestroy<trio::MemoryStream> > stream;
        std::unique_ptr<dna::DNA> dnaInstance;
};

}  // namespace

TEST_F(GeometryFilteringTest, IncludeBlendShapeTargets) {
    dna::FilteredInputArchive archive{stream.get(), dna::DataLayer::Geometry, 0u, std::numeric_limits<std::uint16_t>::max(),
                                      &amr};
    archive >> *dnaInstance;

    ASSERT_FALSE(dnaInstance->geometry.meshes.size() == 0ul);
    for (const auto& mesh : dnaInstance->geometry.meshes) {
        ASSERT_FALSE(mesh.blendShapeTargets.size() == 0ul);
    }
}

TEST_F(GeometryFilteringTest, IgnoreBlendShapeTargets) {
    dna::FilteredInputArchive archive{stream.get(), dna::DataLayer::GeometryWithoutBlendShapes, 0u,
                                      std::numeric_limits<std::uint16_t>::max(), &amr};
    archive >> *dnaInstance;

    ASSERT_FALSE(dnaInstance->geometry.meshes.size() == 0ul);
    for (const auto& mesh : dnaInstance->geometry.meshes) {
        ASSERT_TRUE(mesh.blendShapeTargets.size() == 0ul);
    }
}
