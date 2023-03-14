// Copyright Epic Games, Inc. All Rights Reserved.

#include "rltests/Defs.h"
#include "rltests/dna/DNAFixtures.h"

#include "riglogic/RigLogic.h"

#include <pma/resources/AlignedMemoryResource.h>

namespace {

class RigLogicTest : public ::testing::Test {
    protected:
        void SetUp() override {
            const auto bytes = rltests::raw::getBytes();
            stream = pma::makeScoped<trio::MemoryStream>();
            stream->write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            stream->seek(0);

            reader = dna::StreamReader::create(stream.get());
            reader->read();

            rigLogic = rl4::RigLogic::create(reader);
            rigInstance = rl4::RigInstance::create(rigLogic, &memRes);
        }

        void TearDown() override {
            rl4::RigInstance::destroy(rigInstance);
            rl4::RigLogic::destroy(rigLogic);
            dna::StreamReader::destroy(reader);
        }

    protected:
        pma::AlignedMemoryResource memRes;
        pma::ScopedPtr<trio::MemoryStream, pma::FactoryDestroy<trio::MemoryStream> > stream;
        dna::StreamReader* reader;
        rl4::RigLogic* rigLogic;
        rl4::RigInstance* rigInstance;
};

}  // namespace

TEST_F(RigLogicTest, EvaluateRigInstance) {
    // Try both approaches
    float guiControls[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
    rigInstance->setGUIControlValues(static_cast<const float*>(guiControls));
    rigLogic->mapGUIToRawControls(rigInstance);
    // Regardless that this overwrites the computed gui to raw values
    float rawControls[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
    rigInstance->setRawControlValues(static_cast<const float*>(rawControls));

    rigLogic->calculate(rigInstance);

    ASSERT_EQ(rigInstance->getRawJointOutputs().size(), reader->getJointRowCount());
    ASSERT_EQ(rigInstance->getBlendShapeOutputs().size(), reader->getBlendShapeChannelCount());
    ASSERT_EQ(rigInstance->getAnimatedMapOutputs().size(), reader->getAnimatedMapCount());
}

TEST_F(RigLogicTest, AccessJointVariableAttributeIndices) {
    for (std::uint16_t lod = 0u; lod < rigLogic->getLODCount(); ++lod) {
        ASSERT_EQ(rigLogic->getJointVariableAttributeIndices(lod),
                  (rl4::ConstArrayView<std::uint16_t>{rltests::decoded::jointVariableIndices[0ul][lod]}));
    }
}

TEST_F(RigLogicTest, DumpStateThenRestore) {
    float guiControls[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};

    auto dumpedState = pma::makeScoped<trio::MemoryStream>();
    rigLogic->dump(dumpedState.get());
    dumpedState->seek(0);
    auto cloneRigLogic = rl4::RigLogic::restore(dumpedState.get());
    auto cloneRigInstance = rl4::RigInstance::create(cloneRigLogic);

    for (std::uint16_t lod = 0u; lod < rigLogic->getLODCount(); ++lod) {
        rigInstance->setLOD(lod);
        rigInstance->setGUIControlValues(static_cast<const float*>(guiControls));
        rigLogic->mapGUIToRawControls(rigInstance);
        rigLogic->calculate(rigInstance);

        cloneRigInstance->setLOD(lod);
        cloneRigInstance->setGUIControlValues(static_cast<const float*>(guiControls));
        cloneRigLogic->mapGUIToRawControls(cloneRigInstance);
        cloneRigLogic->calculate(cloneRigInstance);

        auto origJointOutputs = rigInstance->getRawJointOutputs();
        auto origBlendShapeOutputs = rigInstance->getBlendShapeOutputs();
        auto origAnimatedMapOutputs = rigInstance->getAnimatedMapOutputs();

        auto cloneJointOutputs = cloneRigInstance->getRawJointOutputs();
        auto cloneBlendShapeOutputs = cloneRigInstance->getBlendShapeOutputs();
        auto cloneAnimatedMapOutputs = cloneRigInstance->getAnimatedMapOutputs();

        ASSERT_EQ(origJointOutputs, cloneJointOutputs);
        ASSERT_EQ(origBlendShapeOutputs, cloneBlendShapeOutputs);
        ASSERT_EQ(origAnimatedMapOutputs, cloneAnimatedMapOutputs);
    }

    rl4::RigInstance::destroy(cloneRigInstance);
    rl4::RigLogic::destroy(cloneRigLogic);
}
