// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"

namespace UE::Chaos::ClothAsset
{
	/**
	 * Cloth collection reserved attribute names.
	 */
	namespace ClothCollectionAttribute
	{
		// Lods Group
		inline const FName PhysicsAssetPathName(TEXT("PhysicsAssetPathName"));
		inline const FName SkeletalMeshPathName(TEXT("SkeletalMeshPathName"));

		// Solvers Group
		inline const FName SolverGravity(TEXT("SolverGravity"));
		inline const FName SolverAirDamping(TEXT("SolverAirDamping"));
		inline const FName SolverSubSteps(TEXT("SolverSubSteps"));
		inline const FName SolverTimeStep(TEXT("SolverTimeStep"));

		// Fabrics Group
		inline const FName FabricBendingStiffness(TEXT("FabricBendingStiffness"));
		inline const FName FabricBucklingStiffness(TEXT("FabricBucklingStiffness"));
		inline const FName FabricStretchStiffness(TEXT("FabricStretchStiffness"));
		inline const FName FabricBucklingRatio(TEXT("FabricBucklingRatio"));
		inline const FName FabricDensity(TEXT("FabricClothDensity"));
		inline const FName FabricFriction(TEXT("FabricClothFriction"));
		inline const FName FabricDamping(TEXT("FabricClothDamping"));
		inline const FName FabricPressure(TEXT("FabricPressure"));
		inline const FName FabricLayer(TEXT("FabricLayer"));
		inline const FName FabricCollisionThickness(TEXT("FabricollisionThickness"));

		// Seam Group
		inline const FName SeamStitchStart(TEXT("SeamStitchStart"));
		inline const FName SeamStitchEnd(TEXT("SeamStitchEnd"));

		// Seam Stitches Group
		inline const FName SeamStitch2DEndIndices(TEXT("SeamStitch2DEndIndices"));
		inline const FName SeamStitch3DIndex(TEXT("SeamStitch3DIndex"));

		// Sim Patterns Group
		inline const FName SimVertices2DStart(TEXT("SimVertices2DStart"));
		inline const FName SimVertices2DEnd(TEXT("SimVertices2DEnd"));
		inline const FName SimFacesStart(TEXT("SimFacesStart"));
		inline const FName SimFacesEnd(TEXT("SimFacesEnd"));
		inline const FName SimPatternFabric(TEXT("SimPatternFabric"));

		// Render Patterns Group
		inline const FName RenderVerticesStart(TEXT("RenderVerticesStart"));
		inline const FName RenderVerticesEnd(TEXT("RenderVerticesEnd"));
		inline const FName RenderFacesStart(TEXT("RenderFacesStart"));
		inline const FName RenderFacesEnd(TEXT("RenderFacesEnd"));
		inline const FName RenderMaterialPathName(TEXT("RenderMaterialPathName"));
		inline const FName RenderDeformerNumInfluences(TEXT("RenderDeformerNumInfluences"));

		// Sim Faces Group
		inline const FName SimIndices2D(TEXT("SimIndices2D"));
		inline const FName SimIndices3D(TEXT("SimIndices3D"));

		// Sim Vertices 2D Group
		inline const FName SimPosition2D(TEXT("SimPosition2D"));
		inline const FName SimVertex3DLookup(TEXT("SimVertex3DLookup"));

		// Sim Vertices 3D Group
		inline const FName SimPosition3D(TEXT("SimPosition3D"));
		inline const FName SimNormal(TEXT("SimNormal"));
		inline const FName SimBoneIndices(TEXT("SimBoneIndices"));
		inline const FName SimBoneWeights(TEXT("SimBoneWeights"));
		inline const FName TetherKinematicIndex(TEXT("TetherKinematicIndex"));
		inline const FName TetherReferenceLength(TEXT("TetherReferenceLength"));
		inline const FName SimVertex2DLookup(TEXT("SimVertex2DLookup"));
		inline const FName SeamStitchLookup(TEXT("SeamStitchLookup"));

		// Render Faces Group
		inline const FName RenderIndices(TEXT("RenderIndices"));

		// Render Vertices Group
		inline const FName RenderPosition(TEXT("RenderPosition"));
		inline const FName RenderNormal(TEXT("RenderNormal"));
		inline const FName RenderTangentU(TEXT("RenderTangentU"));
		inline const FName RenderTangentV(TEXT("RenderTangentV"));
		inline const FName RenderUVs(TEXT("RenderUVs"));
		inline const FName RenderColor(TEXT("RenderColor"));
		inline const FName RenderBoneIndices(TEXT("RenderBoneIndices"));
		inline const FName RenderBoneWeights(TEXT("RenderBoneWeights"));
		inline const FName RenderDeformerPositionBaryCoordsAndDist(TEXT("RenderDeformerPositionBaryCoordsAndDist"));
		inline const FName RenderDeformerNormalBaryCoordsAndDist(TEXT("RenderDeformerNormalBaryCoordsAndDist"));
		inline const FName RenderDeformerTangentBaryCoordsAndDist(TEXT("RenderDeformerTangentBaryCoordsAndDist"));
		inline const FName RenderDeformerSimIndices3D(TEXT("RenderDeformerSimIndices3D"));
		inline const FName RenderDeformerWeight(TEXT("RenderDeformerWeight"));
		inline const FName RenderDeformerSkinningBlend(TEXT("RenderDeformerSkinningBlend"));
	}
}  // End namespace UE::Chaos::ClothAsset
