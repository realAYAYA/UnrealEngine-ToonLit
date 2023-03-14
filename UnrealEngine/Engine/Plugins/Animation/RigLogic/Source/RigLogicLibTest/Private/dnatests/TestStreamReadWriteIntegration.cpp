// Copyright Epic Games, Inc. All Rights Reserved.

#include "dnatests/TestStreamReadWriteIntegration.h"

#include "dnatests/Defs.h"
#include "dnatests/Fixtures.h"

#include "dna/DataLayer.h"
#include "dna/DNA.h"
#include "dna/StreamReader.h"
#include "dna/StreamWriter.h"

#include <pma/resources/AlignedMemoryResource.h>

StreamReadWriteIntegrationTest::~StreamReadWriteIntegrationTest() = default;

namespace dna {

static void verifyDescriptor(DescriptorReader* reader, const LODParameters& params) {
    ASSERT_EQ(reader->getName(), StringView{decoded::name});
    ASSERT_EQ(reader->getArchetype(), decoded::archetype);
    ASSERT_EQ(reader->getGender(), decoded::gender);
    ASSERT_EQ(reader->getAge(), decoded::age);

    const auto metaDataCount = reader->getMetaDataCount();
    ASSERT_EQ(metaDataCount, 2u);
    for (std::uint32_t i = 0u; i < metaDataCount; ++i) {
        const auto key = reader->getMetaDataKey(i);
        const auto value = reader->getMetaDataValue(key);
        ASSERT_EQ(key, StringView{decoded::metadata[i].first});
        ASSERT_EQ(value, StringView{decoded::metadata[i].second});
    }

    ASSERT_EQ(reader->getTranslationUnit(), decoded::translationUnit);
    ASSERT_EQ(reader->getRotationUnit(), decoded::rotationUnit);

    const auto coordinateSystem = reader->getCoordinateSystem();
    ASSERT_EQ(coordinateSystem.xAxis, decoded::coordinateSystem.xAxis);
    ASSERT_EQ(coordinateSystem.yAxis, decoded::coordinateSystem.yAxis);
    ASSERT_EQ(coordinateSystem.zAxis, decoded::coordinateSystem.zAxis);

    ASSERT_EQ(reader->getLODCount(), decoded::lodCount[params.maxLOD]);
    ASSERT_EQ(reader->getDBMaxLOD(), decoded::maxLODs[params.maxLOD]);
    ASSERT_EQ(reader->getDBComplexity(), StringView{decoded::complexity});
    ASSERT_EQ(reader->getDBName(), StringView{decoded::dbName});
}

static void verifyDefinition(DefinitionReader* reader, const LODParameters& params) {
    const auto guiControlCount = reader->getGUIControlCount();
    ASSERT_EQ(guiControlCount, decoded::guiControlNames.size());
    for (std::uint16_t i = 0u; i < guiControlCount; ++i) {
        ASSERT_EQ(reader->getGUIControlName(i), StringView{decoded::guiControlNames[i]});
    }

    const auto rawControlCount = reader->getRawControlCount();
    ASSERT_EQ(rawControlCount, decoded::rawControlNames.size());
    for (std::uint16_t i = 0u; i < rawControlCount; ++i) {
        ASSERT_EQ(reader->getRawControlName(i), StringView{decoded::rawControlNames[i]});
    }

    ASSERT_EQ(reader->getJointCount(), decoded::jointNames[params.maxLOD][0ul].size());
    const auto& expectedJointNames = decoded::jointNames[params.maxLOD][params.currentLOD];
    const auto jointIndices = reader->getJointIndicesForLOD(params.currentLOD);
    ASSERT_EQ(jointIndices.size(), expectedJointNames.size());
    for (std::size_t i = 0ul; i < jointIndices.size(); ++i) {
        ASSERT_EQ(reader->getJointName(jointIndices[i]), StringView{expectedJointNames[i]});
    }

    for (std::uint16_t i = 0u; i < reader->getJointCount(); ++i) {
        ASSERT_EQ(reader->getJointParentIndex(i), decoded::jointHierarchy[params.maxLOD][i]);
    }

    ASSERT_EQ(reader->getBlendShapeChannelCount(), decoded::blendShapeNames[params.maxLOD][0ul].size());
    const auto& expectedBlendShapeNames = decoded::blendShapeNames[params.maxLOD][params.currentLOD];
    const auto blendShapeIndices = reader->getBlendShapeChannelIndicesForLOD(params.currentLOD);
    ASSERT_EQ(blendShapeIndices.size(), expectedBlendShapeNames.size());
    for (std::size_t i = 0ul; i < blendShapeIndices.size(); ++i) {
        ASSERT_EQ(reader->getBlendShapeChannelName(blendShapeIndices[i]), StringView{expectedBlendShapeNames[i]});
    }

    ASSERT_EQ(reader->getAnimatedMapCount(), decoded::animatedMapNames[params.maxLOD][0ul].size());
    const auto& expectedAnimatedMapNames = decoded::animatedMapNames[params.maxLOD][params.currentLOD];
    const auto animatedMapIndices = reader->getAnimatedMapIndicesForLOD(params.currentLOD);
    ASSERT_EQ(animatedMapIndices.size(), expectedAnimatedMapNames.size());
    for (std::size_t i = 0ul; i < animatedMapIndices.size(); ++i) {
        ASSERT_EQ(reader->getAnimatedMapName(animatedMapIndices[i]), StringView{expectedAnimatedMapNames[i]});
    }

    ASSERT_EQ(reader->getMeshCount(), decoded::meshNames[params.maxLOD][0ul].size());
    const auto& expectedMeshNames = decoded::meshNames[params.maxLOD][params.currentLOD];
    const auto meshIndices = reader->getMeshIndicesForLOD(params.currentLOD);
    ASSERT_EQ(meshIndices.size(), expectedMeshNames.size());
    for (std::size_t i = 0ul; i < meshIndices.size(); ++i) {
        ASSERT_EQ(reader->getMeshName(meshIndices[i]), StringView{expectedMeshNames[i]});
    }

    ASSERT_EQ(reader->getMeshBlendShapeChannelMappingCount(), decoded::meshBlendShapeIndices[params.maxLOD][0ul].size());
    const auto meshBlendShapeIndices = reader->getMeshBlendShapeChannelMappingIndicesForLOD(params.currentLOD);
    const auto& expectedMeshBlendShapeIndices = decoded::meshBlendShapeIndices[params.maxLOD][params.currentLOD];
    ASSERT_EQ(meshBlendShapeIndices, ConstArrayView<std::uint16_t>{expectedMeshBlendShapeIndices});

    const auto& expectedNeutralJointTranslations = decoded::neutralJointTranslations[params.maxLOD][params.currentLOD];
    ASSERT_EQ(jointIndices.size(), expectedNeutralJointTranslations.size());
    for (std::size_t i = 0ul; i < jointIndices.size(); ++i) {
        ASSERT_EQ(reader->getNeutralJointTranslation(jointIndices[i]), expectedNeutralJointTranslations[i]);
    }

    const auto& expectedNeutralJointRotations = decoded::neutralJointRotations[params.maxLOD][params.currentLOD];
    ASSERT_EQ(jointIndices.size(), expectedNeutralJointRotations.size());
    for (std::size_t i = 0ul; i < jointIndices.size(); ++i) {
        ASSERT_EQ(reader->getNeutralJointRotation(jointIndices[i]), expectedNeutralJointRotations[i]);
    }
}

static void verifyBehavior(BehaviorReader* reader, const LODParameters& params) {
    const auto guiToRawInputIndices = reader->getGUIToRawInputIndices();
    const auto& expectedG2RInputIndices = decoded::conditionalInputIndices[0ul][0ul];
    ASSERT_EQ(guiToRawInputIndices, ConstArrayView<std::uint16_t>{expectedG2RInputIndices});

    const auto guiToRawOutputIndices = reader->getGUIToRawOutputIndices();
    const auto& expectedG2ROutputIndices = decoded::conditionalOutputIndices[0ul][0ul];
    ASSERT_EQ(guiToRawOutputIndices, ConstArrayView<std::uint16_t>{expectedG2ROutputIndices});

    const auto guiToRawFromValues = reader->getGUIToRawFromValues();
    const auto& expectedG2RFromValues = decoded::conditionalFromValues[0ul][0ul];
    ASSERT_EQ(guiToRawFromValues, ConstArrayView<float>{expectedG2RFromValues});

    const auto guiToRawToValues = reader->getGUIToRawToValues();
    const auto& expectedG2RToValues = decoded::conditionalToValues[0ul][0ul];
    ASSERT_EQ(guiToRawToValues, ConstArrayView<float>{expectedG2RToValues});

    const auto guiToRawSlopeValues = reader->getGUIToRawSlopeValues();
    const auto& expectedG2RSlopeValues = decoded::conditionalSlopeValues[0ul][0ul];
    ASSERT_EQ(guiToRawSlopeValues, ConstArrayView<float>{expectedG2RSlopeValues});

    const auto guiToRawCutValues = reader->getGUIToRawCutValues();
    const auto& expectedG2RCutValues = decoded::conditionalCutValues[0ul][0ul];
    ASSERT_EQ(guiToRawCutValues, ConstArrayView<float>{expectedG2RCutValues});

    const auto psdRowIndices = reader->getPSDRowIndices();
    ASSERT_EQ(psdRowIndices, ConstArrayView<std::uint16_t>{decoded::psdRowIndices});

    const auto psdColumnIndices = reader->getPSDColumnIndices();
    ASSERT_EQ(psdColumnIndices, ConstArrayView<std::uint16_t>{decoded::psdColumnIndices});

    const auto psdValues = reader->getPSDValues();
    ASSERT_EQ(psdValues, ConstArrayView<float>{decoded::psdValues});

    ASSERT_EQ(reader->getPSDCount(), decoded::psdCount);
    ASSERT_EQ(reader->getJointRowCount(), decoded::jointRowCount[params.maxLOD]);
    ASSERT_EQ(reader->getJointColumnCount(), decoded::jointColumnCount);

    const auto jointVariableAttrIndices = reader->getJointVariableAttributeIndices(params.currentLOD);
    const auto& expectedJointVariableAttrIndices = decoded::jointVariableIndices[params.maxLOD][params.currentLOD];
    ASSERT_EQ(jointVariableAttrIndices, ConstArrayView<std::uint16_t>{expectedJointVariableAttrIndices});

    const auto jointGroupCount = reader->getJointGroupCount();
    ASSERT_EQ(jointGroupCount, decoded::jointGroupLODs.size());

    for (std::uint16_t i = 0u; i < jointGroupCount; ++i) {
        const auto& expectedLODs = decoded::jointGroupLODs[i][params.maxLOD];
        ASSERT_EQ(reader->getJointGroupLODs(i), ConstArrayView<std::uint16_t>{expectedLODs});

        const auto& expectedInputIndices = decoded::jointGroupInputIndices[i][params.maxLOD][0ul];
        ASSERT_EQ(reader->getJointGroupInputIndices(i), ConstArrayView<std::uint16_t>{expectedInputIndices});

        const auto outputIndices = reader->getJointGroupOutputIndices(i);
        ASSERT_EQ(outputIndices.size(), expectedLODs[0ul]);

        ConstArrayView<std::uint16_t> outputIndicesForLOD{outputIndices.data(), expectedLODs[params.currentLOD]};
        const auto& expectedOutputIndices = decoded::jointGroupOutputIndices[i][params.maxLOD][params.currentLOD];
        ASSERT_EQ(outputIndicesForLOD, ConstArrayView<std::uint16_t>{expectedOutputIndices});

        const auto values = reader->getJointGroupValues(i);
        ASSERT_EQ(values.size(), expectedLODs[0ul] * expectedInputIndices.size());

        ConstArrayView<float> valuesForLOD{values.data(), expectedLODs[params.currentLOD] * expectedInputIndices.size()};
        const auto& expectedValues = decoded::jointGroupValues[i][params.maxLOD][params.currentLOD];
        ASSERT_EQ(valuesForLOD, ConstArrayView<float>{expectedValues});

        const auto& expectedJointIndices = decoded::jointGroupJointIndices[i][params.maxLOD][0ul];
        ASSERT_EQ(reader->getJointGroupJointIndices(i), ConstArrayView<std::uint16_t>{expectedJointIndices});
    }

    ASSERT_EQ(reader->getBlendShapeChannelLODs(),
              ConstArrayView<std::uint16_t>{decoded::blendShapeLODs[params.maxLOD]});

    const auto blendShapeChannelInputIndices = reader->getBlendShapeChannelInputIndices();
    ASSERT_EQ(blendShapeChannelInputIndices.size(), decoded::blendShapeLODs[params.maxLOD][0ul]);
    ConstArrayView<std::uint16_t> blendShapeInputIndicesForLOD{
        blendShapeChannelInputIndices.data(),
        decoded::blendShapeLODs[params.maxLOD][params.currentLOD]
    };
    ASSERT_EQ(blendShapeInputIndicesForLOD,
              ConstArrayView<std::uint16_t>{decoded::blendShapeInputIndices[params.maxLOD][params.currentLOD]});

    const auto blendShapeChannelOutputIndices = reader->getBlendShapeChannelOutputIndices();
    ASSERT_EQ(blendShapeChannelOutputIndices.size(), decoded::blendShapeLODs[params.maxLOD][0ul]);
    ConstArrayView<std::uint16_t> blendShapeOutputIndicesForLOD{
        blendShapeChannelOutputIndices.data(),
        decoded::blendShapeLODs[params.maxLOD][params.currentLOD]
    };
    ASSERT_EQ(blendShapeOutputIndicesForLOD,
              ConstArrayView<std::uint16_t>{decoded::blendShapeOutputIndices[params.maxLOD][params.currentLOD]});

    ASSERT_EQ(reader->getAnimatedMapLODs(),
              ConstArrayView<std::uint16_t>{decoded::animatedMapLODs[params.maxLOD]});

    ASSERT_EQ(reader->getAnimatedMapCount(), decoded::animatedMapCount[params.maxLOD]);

    const auto animatdMapLOD = decoded::animatedMapLODs[params.maxLOD][params.currentLOD];

    const auto animatedMapInputIndices = reader->getAnimatedMapInputIndices();
    ASSERT_EQ(animatedMapInputIndices.size(), decoded::animatedMapLODs[params.maxLOD][0ul]);
    ConstArrayView<std::uint16_t> animatedMapInputIndicesForLOD{animatedMapInputIndices.data(), animatdMapLOD};
    ASSERT_EQ(animatedMapInputIndicesForLOD,
              ConstArrayView<std::uint16_t>{decoded::conditionalInputIndices[params.maxLOD][params.currentLOD]});

    const auto animatedMapOutputIndices = reader->getAnimatedMapOutputIndices();
    ASSERT_EQ(animatedMapOutputIndices.size(), decoded::animatedMapLODs[params.maxLOD][0ul]);
    ConstArrayView<std::uint16_t> animatedMapOutputIndicesForLOD{animatedMapOutputIndices.data(), animatdMapLOD};
    ASSERT_EQ(animatedMapOutputIndicesForLOD,
              ConstArrayView<std::uint16_t>{decoded::conditionalOutputIndices[params.maxLOD][params.currentLOD]});

    const auto animatedMapFromValues = reader->getAnimatedMapFromValues();
    ASSERT_EQ(animatedMapFromValues.size(), decoded::animatedMapLODs[params.maxLOD][0ul]);
    ConstArrayView<float> animatedMapFromValuesForLOD{animatedMapFromValues.data(), animatdMapLOD};
    ASSERT_EQ(animatedMapFromValuesForLOD,
              ConstArrayView<float>{decoded::conditionalFromValues[params.maxLOD][params.currentLOD]});

    const auto animatedMapToValues = reader->getAnimatedMapToValues();
    ASSERT_EQ(animatedMapToValues.size(), decoded::animatedMapLODs[params.maxLOD][0ul]);
    ConstArrayView<float> animatedMapToValuesForLOD{animatedMapToValues.data(), animatdMapLOD};
    ASSERT_EQ(animatedMapToValuesForLOD,
              ConstArrayView<float>{decoded::conditionalToValues[params.maxLOD][params.currentLOD]});

    const auto animatedMapSlopeValues = reader->getAnimatedMapSlopeValues();
    ASSERT_EQ(animatedMapSlopeValues.size(), decoded::animatedMapLODs[params.maxLOD][0ul]);
    ConstArrayView<float> animatedMapSlopeValuesForLOD{animatedMapSlopeValues.data(), animatdMapLOD};
    ASSERT_EQ(animatedMapSlopeValuesForLOD,
              ConstArrayView<float>{decoded::conditionalSlopeValues[params.maxLOD][params.currentLOD]});

    const auto animatedMapCutValues = reader->getAnimatedMapCutValues();
    ASSERT_EQ(animatedMapCutValues.size(), decoded::animatedMapLODs[params.maxLOD][0ul]);
    ConstArrayView<float> animatedMapCutValuesForLOD{animatedMapCutValues.data(), animatdMapLOD};
    ASSERT_EQ(animatedMapCutValuesForLOD,
              ConstArrayView<float>{decoded::conditionalCutValues[params.maxLOD][params.currentLOD]});
}

static void verifyGeometry(GeometryReader* reader, const LODParameters& params) {
    const auto meshCount = reader->getMeshCount();
    ASSERT_EQ(meshCount, decoded::meshCount[params.maxLOD]);
    for (std::uint16_t meshIndex = 0u; meshIndex < meshCount; ++meshIndex) {
        const auto vertexPositionCount = reader->getVertexPositionCount(meshIndex);
        ASSERT_EQ(vertexPositionCount, decoded::vertexPositions[params.maxLOD][meshIndex].size());
        for (std::uint32_t vertexIndex = 0u; vertexIndex < vertexPositionCount; ++vertexIndex) {
            ASSERT_EQ(reader->getVertexPosition(meshIndex, vertexIndex),
                      decoded::vertexPositions[params.maxLOD][meshIndex][vertexIndex]);
        }

        const auto vertexTextureCoordinateCount = reader->getVertexTextureCoordinateCount(meshIndex);
        ASSERT_EQ(vertexTextureCoordinateCount, decoded::vertexTextureCoordinates[params.maxLOD][meshIndex].size());
        for (std::uint32_t texCoordIndex = 0u; texCoordIndex < vertexTextureCoordinateCount; ++texCoordIndex) {
            const auto& textureCoordinate = reader->getVertexTextureCoordinate(meshIndex, texCoordIndex);
            const auto& expectedTextureCoordinate =
                decoded::vertexTextureCoordinates[params.maxLOD][meshIndex][texCoordIndex];
            ASSERT_EQ(textureCoordinate.u, expectedTextureCoordinate.u);
            ASSERT_EQ(textureCoordinate.v, expectedTextureCoordinate.v);
        }

        const auto vertexNormalCount = reader->getVertexNormalCount(meshIndex);
        ASSERT_EQ(vertexNormalCount, decoded::vertexNormals[params.maxLOD][meshIndex].size());
        for (std::uint32_t normalIndex = 0u; normalIndex < vertexNormalCount; ++normalIndex) {
            ASSERT_EQ(reader->getVertexNormal(meshIndex, normalIndex),
                      decoded::vertexNormals[params.maxLOD][meshIndex][normalIndex]);
        }

        const auto vertexLayoutCount = reader->getVertexLayoutCount(meshIndex);
        ASSERT_EQ(vertexLayoutCount, decoded::vertexLayouts[params.maxLOD][meshIndex].size());
        for (std::uint32_t layoutIndex = 0u; layoutIndex < vertexLayoutCount; ++layoutIndex) {
            const auto& layout = reader->getVertexLayout(meshIndex, layoutIndex);
            const auto& expectedLayout = decoded::vertexLayouts[params.maxLOD][meshIndex][layoutIndex];
            ASSERT_EQ(layout.position, expectedLayout.position);
            ASSERT_EQ(layout.textureCoordinate, expectedLayout.textureCoordinate);
            ASSERT_EQ(layout.normal, expectedLayout.normal);
        }

        const auto faceCount = reader->getFaceCount(meshIndex);
        ASSERT_EQ(faceCount, decoded::faces[params.maxLOD][meshIndex].size());
        for (std::uint32_t faceIndex = 0u; faceIndex < faceCount; ++faceIndex) {
            ASSERT_EQ(reader->getFaceVertexLayoutIndices(meshIndex, faceIndex),
                      ConstArrayView<std::uint32_t>{decoded::faces[params.maxLOD][meshIndex][faceIndex]});
        }

        ASSERT_EQ(reader->getMaximumInfluencePerVertex(meshIndex),
                  decoded::maxInfluencePerVertex[params.maxLOD][meshIndex]);

        ASSERT_EQ(reader->getSkinWeightsCount(meshIndex), decoded::skinWeightsValues[params.maxLOD][meshIndex].size());
        for (std::uint32_t vertexIndex = 0u; vertexIndex < vertexPositionCount; ++vertexIndex) {
            const auto skinWeights = reader->getSkinWeightsValues(meshIndex, vertexIndex);
            const auto& expectedSkinWeights = decoded::skinWeightsValues[params.maxLOD][meshIndex][vertexIndex];
            ASSERT_EQ(skinWeights, ConstArrayView<float>{expectedSkinWeights});

            const auto jointIndices = reader->getSkinWeightsJointIndices(meshIndex, vertexIndex);
            const auto& expectedJointIndices = decoded::skinWeightsJointIndices[params.maxLOD][meshIndex][vertexIndex];
            ASSERT_EQ(jointIndices, ConstArrayView<std::uint16_t>{expectedJointIndices});
        }

        const auto blendShapeCount = reader->getBlendShapeTargetCount(meshIndex);
        ASSERT_EQ(blendShapeCount, decoded::correctiveBlendShapeDeltas[params.maxLOD][meshIndex].size());
        for (std::uint16_t blendShapeTargetIndex = 0u; blendShapeTargetIndex < blendShapeCount; ++blendShapeTargetIndex) {
            const auto channelIndex = reader->getBlendShapeChannelIndex(meshIndex, blendShapeTargetIndex);
            ASSERT_EQ(channelIndex, decoded::correctiveBlendShapeIndices[params.maxLOD][meshIndex][blendShapeTargetIndex]);

            const auto deltaCount = reader->getBlendShapeTargetDeltaCount(meshIndex, blendShapeTargetIndex);
            ASSERT_EQ(deltaCount,
                      decoded::correctiveBlendShapeDeltas[params.maxLOD][meshIndex][blendShapeTargetIndex].size());

            for (std::uint32_t deltaIndex = 0u; deltaIndex < deltaCount; ++deltaIndex) {
                const auto& delta = reader->getBlendShapeTargetDelta(meshIndex, blendShapeTargetIndex, deltaIndex);
                const auto& expectedDelta =
                    decoded::correctiveBlendShapeDeltas[params.maxLOD][meshIndex][blendShapeTargetIndex][deltaIndex];
                ASSERT_EQ(delta, expectedDelta);
            }

            const auto vertexIndices = reader->getBlendShapeTargetVertexIndices(meshIndex, blendShapeTargetIndex);
            const auto& expectedVertexIndices =
                decoded::correctiveBlendShapeVertexIndices[params.maxLOD][meshIndex][blendShapeTargetIndex];
            ASSERT_EQ(vertexIndices, ConstArrayView<std::uint32_t>{expectedVertexIndices});
        }
    }
}

static void assertReaderHasAllData(Reader* reader, const LODParameters& params) {
    verifyDescriptor(reader, params);
    verifyDefinition(reader, params);
    verifyBehavior(reader, params);
    verifyGeometry(reader, params);
}

TEST_P(StreamReadWriteIntegrationTest, VerifyAllDNADataAfterSetFromIsUsed) {
    pma::AlignedMemoryResource memRes;

    const auto bytes = raw::getBytes();
    auto source = pma::makeScoped<trio::MemoryStream>();
    source->write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    source->seek(0);

    auto sourceReader = StreamReader::create(source.get(), DataLayer::All, 0u, &memRes);
    sourceReader->read();

    auto clone = pma::makeScoped<trio::MemoryStream>();
    auto cloneWriter = StreamWriter::create(clone.get(), &memRes);
    cloneWriter->setFrom(sourceReader);
    cloneWriter->write();

    clone->seek(0ul);
    auto cloneReader = StreamReader::create(clone.get(), DataLayer::All, params.maxLOD, &memRes);
    cloneReader->read();

    assertReaderHasAllData(cloneReader, params);

    StreamReader::destroy(cloneReader);
    StreamWriter::destroy(cloneWriter);
    StreamReader::destroy(sourceReader);
}

INSTANTIATE_TEST_SUITE_P(StreamReadWriteIntegrationTest, StreamReadWriteIntegrationTest, ::testing::Values(
                             // maxLOD, currentLOD
                             LODParameters{0u, 0u},
                             LODParameters{0u, 1u},
                             LODParameters{1u, 0u}
                             ));

}  // namespace dna
