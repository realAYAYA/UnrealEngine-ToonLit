// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "dnatests/Defs.h"

#include "dna/DNA.h"
#include "dna/Reader.h"

#include <pma/TypeDefs.h>

namespace dna {

namespace raw {

extern const unsigned char header[39];
extern const unsigned char descriptor[87];
extern const unsigned char definition[796];
extern const unsigned char controls[2];
extern const unsigned char conditionals[324];
extern const unsigned char psds[204];
extern const unsigned char joints[444];
extern const unsigned char blendshapes[44];
extern const unsigned char animatedmaps[8];
extern const unsigned char geometry[1080];
extern const unsigned char footer[3];

std::vector<unsigned char> getBytes();

}  // namespace raw

namespace decoded {

using VectorOfCharStringMatrix = pma::Vector<pma::Matrix<pma::String<char> > >;
using StringPair = std::pair<pma::String<char>, pma::String<char> >;

// Descriptor
extern const pma::String<char> name;
extern const Archetype archetype;
extern const Gender gender;
extern const std::uint16_t age;

extern const pma::Vector<StringPair> metadata;
extern const TranslationUnit translationUnit;
extern const RotationUnit rotationUnit;
extern const CoordinateSystem coordinateSystem;
extern const std::uint16_t lodCount[3ul];
extern const std::uint16_t maxLODs[3ul];
extern const pma::String<char> complexity;
extern const pma::String<char> dbName;

// Definition
extern const pma::Vector<pma::String<char> > guiControlNames;
extern const pma::Vector<pma::String<char> > rawControlNames;
extern const VectorOfCharStringMatrix jointNames;
extern const VectorOfCharStringMatrix blendShapeNames;
extern const VectorOfCharStringMatrix animatedMapNames;
extern const VectorOfCharStringMatrix meshNames;
extern const pma::Vector<pma::Matrix<std::uint16_t> > meshBlendShapeIndices;
extern const pma::Matrix<std::uint16_t> jointHierarchy;
extern const pma::Vector<pma::Matrix<Vector3> > neutralJointTranslations;
extern const pma::Vector<pma::Matrix<Vector3> > neutralJointRotations;

// Behavior
extern const std::uint16_t guiControlCount;
extern const std::uint16_t rawControlCount;
extern const std::uint16_t psdCount;
extern const pma::Vector<pma::Matrix<std::uint16_t> > conditionalInputIndices;
extern const pma::Vector<pma::Matrix<std::uint16_t> > conditionalOutputIndices;
extern const pma::Vector<pma::Matrix<float> > conditionalFromValues;
extern const pma::Vector<pma::Matrix<float> > conditionalToValues;
extern const pma::Vector<pma::Matrix<float> > conditionalSlopeValues;
extern const pma::Vector<pma::Matrix<float> > conditionalCutValues;
extern const pma::Vector<std::uint16_t> psdRowIndices;
extern const pma::Vector<std::uint16_t> psdColumnIndices;
extern const pma::Vector<float> psdValues;
extern const pma::Vector<std::uint16_t> jointRowCount;
extern const std::uint16_t jointColumnCount;
extern const pma::Vector<pma::Matrix<std::uint16_t> > jointVariableIndices;
extern const pma::Vector<pma::Matrix<std::uint16_t> > jointGroupLODs;
extern const pma::Matrix<pma::Matrix<std::uint16_t> > jointGroupInputIndices;
extern const pma::Matrix<pma::Matrix<std::uint16_t> > jointGroupOutputIndices;
extern const pma::Matrix<pma::Matrix<float> > jointGroupValues;
extern const pma::Matrix<pma::Matrix<std::uint16_t> > jointGroupJointIndices;
extern const pma::Matrix<std::uint16_t> blendShapeLODs;
extern const pma::Vector<pma::Matrix<std::uint16_t> > blendShapeInputIndices;
extern const pma::Vector<pma::Matrix<std::uint16_t> > blendShapeOutputIndices;
extern const pma::Vector<std::uint16_t> animatedMapCount;
extern const pma::Matrix<std::uint16_t> animatedMapLODs;

// Geometry
extern const pma::Vector<std::uint32_t> meshCount;
extern const pma::Vector<pma::Matrix<Vector3> > vertexPositions;
extern const pma::Vector<pma::Matrix<TextureCoordinate> > vertexTextureCoordinates;
extern const pma::Vector<pma::Matrix<Vector3> > vertexNormals;
extern const pma::Vector<pma::Matrix<VertexLayout> > vertexLayouts;
extern const pma::Matrix<pma::Matrix<std::uint32_t> > faces;
extern const pma::Matrix<std::uint16_t> maxInfluencePerVertex;
extern const pma::Matrix<pma::Matrix<float> > skinWeightsValues;
extern const pma::Matrix<pma::Matrix<std::uint16_t> > skinWeightsJointIndices;
extern const pma::Vector<pma::Matrix<std::uint16_t> > correctiveBlendShapeIndices;
extern const pma::Matrix<pma::Matrix<Vector3> > correctiveBlendShapeDeltas;
extern const pma::Matrix<pma::Matrix<std::uint32_t> > correctiveBlendShapeVertexIndices;

struct Fixtures {
    static std::size_t lodConstraintToIndex(std::uint16_t maxLOD, std::uint16_t minLOD);
    static RawJoints getJoints(std::uint16_t currentMaxLOD, std::uint16_t currentMinLOD, pma::MemoryResource* memRes);
    static RawBlendShapeChannels getBlendShapes(std::uint16_t currentMaxLOD,
                                                std::uint16_t currentMinLOD,
                                                pma::MemoryResource* memRes);
    static RawConditionalTable getConditionals(std::uint16_t currentMaxLOD,
                                               std::uint16_t currentMinLOD,
                                               pma::MemoryResource* memRes);
    static RawAnimatedMaps getAnimatedMaps(std::uint16_t currentMaxLOD, std::uint16_t currentMinLOD, pma::MemoryResource* memRes);

};

}  // namespace decoded

}  // namespace dna
