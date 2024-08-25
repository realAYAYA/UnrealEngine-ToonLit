// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothCollection.h"
#include "ChaosClothAsset/ClothCollectionGroup.h"
#include "ChaosClothAsset/ClothCollectionAttribute.h"
#include "GeometryCollection/ManagedArrayCollection.h"

namespace UE::Chaos::ClothAsset::Private
{
	// Lods Group
	static const TArray<FName> LodsGroupAttributes =
	{
		ClothCollectionAttribute::PhysicsAssetPathName,
		ClothCollectionAttribute::SkeletalMeshPathName,
	};

	// Solvers Group
	static const TArray<FName> SolverGroupAttributes =
	{
		ClothCollectionAttribute::SolverGravity,
		ClothCollectionAttribute::SolverAirDamping,
		ClothCollectionAttribute::SolverSubSteps,
		ClothCollectionAttribute::SolverTimeStep,
	};

	// Fabrics Group
	static const TArray<FName> SimFabricGroupAttributes =
	{
		ClothCollectionAttribute::FabricBendingStiffness,
		ClothCollectionAttribute::FabricBucklingStiffness,
		ClothCollectionAttribute::FabricStretchStiffness,
		ClothCollectionAttribute::FabricBucklingRatio,
		ClothCollectionAttribute::FabricDensity,
		ClothCollectionAttribute::FabricFriction,
		ClothCollectionAttribute::FabricDamping,
		ClothCollectionAttribute::FabricPressure,
		ClothCollectionAttribute::FabricLayer,
		ClothCollectionAttribute::FabricCollisionThickness,
	};

	// Seam Group
	static const TArray<FName> SeamsGroupAttributes =
	{
		ClothCollectionAttribute::SeamStitchStart,
		ClothCollectionAttribute::SeamStitchEnd
	};

	// Seam Stitches Group
	static const TArray<FName> SeamStitchesGroupAttributes =
	{
		ClothCollectionAttribute::SeamStitch2DEndIndices,
		ClothCollectionAttribute::SeamStitch3DIndex
	};

	// Sim Patterns Group
	static const TArray<FName> SimPatternsGroupAttributes =
	{ 
		ClothCollectionAttribute::SimVertices2DStart,
		ClothCollectionAttribute::SimVertices2DEnd,
		ClothCollectionAttribute::SimFacesStart,
		ClothCollectionAttribute::SimFacesEnd,
		ClothCollectionAttribute::SimPatternFabric,
		
	};

	// Render Patterns Group
	static const TArray<FName> RenderPatternsGroupAttributes =
	{
		ClothCollectionAttribute::RenderVerticesStart,
		ClothCollectionAttribute::RenderVerticesEnd,
		ClothCollectionAttribute::RenderFacesStart,
		ClothCollectionAttribute::RenderFacesEnd,
		ClothCollectionAttribute::RenderDeformerNumInfluences,
		ClothCollectionAttribute::RenderMaterialPathName
	};

	// Sim Faces Group
	static const TArray<FName> SimFacesGroupAttributes =
	{
		ClothCollectionAttribute::SimIndices2D,
		ClothCollectionAttribute::SimIndices3D
	};

	// Sim Vertices 2D Group
	static const TArray<FName> SimVertices2DGroupAttributes =
	{
		ClothCollectionAttribute::SimPosition2D,
		ClothCollectionAttribute::SimVertex3DLookup,
	};

	// Sim Vertices 3D Group
	// NOTE: if you add anything here, you need to implement how to merge it 
	// in CollectionClothSeamsFacade. Otherwise, the data in the lowest vertex index 
	// will survive and the other data will be lost whenever seams are added.
	static const TArray<FName> SimVertices3DGroupAttributes =
	{
		ClothCollectionAttribute::SimPosition3D,
		ClothCollectionAttribute::SimNormal,
		ClothCollectionAttribute::SimBoneIndices,
		ClothCollectionAttribute::SimBoneWeights,
		ClothCollectionAttribute::SimVertex2DLookup,
		ClothCollectionAttribute::SeamStitchLookup
	};

	// Render Faces Group
	static const TArray<FName> RenderFacesGroupAttributes =
	{
		ClothCollectionAttribute::RenderIndices,
	};

	// Render Vertices Group
	static const TArray<FName> RenderVerticesGroupAttributes =
	{
		ClothCollectionAttribute::RenderPosition,
		ClothCollectionAttribute::RenderNormal,
		ClothCollectionAttribute::RenderTangentU,
		ClothCollectionAttribute::RenderTangentV,
		ClothCollectionAttribute::RenderUVs,
		ClothCollectionAttribute::RenderColor,
		ClothCollectionAttribute::RenderBoneIndices,
		ClothCollectionAttribute::RenderBoneWeights,
		ClothCollectionAttribute::RenderDeformerPositionBaryCoordsAndDist,
		ClothCollectionAttribute::RenderDeformerNormalBaryCoordsAndDist,
		ClothCollectionAttribute::RenderDeformerTangentBaryCoordsAndDist,
		ClothCollectionAttribute::RenderDeformerSimIndices3D,
		ClothCollectionAttribute::RenderDeformerWeight,
		ClothCollectionAttribute::RenderDeformerSkinningBlend
	};

	// All fixed attributes for this collection
	static const TMap<FName, TArray<FName>> FixedAttributeNamesMap =
	{
		{ ClothCollectionGroup::Lods, LodsGroupAttributes },
		{ ClothCollectionGroup::Seams, SeamsGroupAttributes },
		{ ClothCollectionGroup::SeamStitches, SeamStitchesGroupAttributes },
		{ ClothCollectionGroup::SimPatterns, SimPatternsGroupAttributes },
		{ ClothCollectionGroup::RenderPatterns, RenderPatternsGroupAttributes },
		{ ClothCollectionGroup::SimFaces, SimFacesGroupAttributes },
		{ ClothCollectionGroup::SimVertices2D, SimVertices2DGroupAttributes },
		{ ClothCollectionGroup::SimVertices3D, SimVertices3DGroupAttributes },
		{ ClothCollectionGroup::RenderFaces, RenderFacesGroupAttributes },
		{ ClothCollectionGroup::RenderVertices, RenderVerticesGroupAttributes },
		{ ClothCollectionGroup::Fabrics, SimFabricGroupAttributes },
		{ ClothCollectionGroup::Solvers, SolverGroupAttributes },
	};
}  // End namespace UE::Chaos::ClothAsset::Private

namespace UE::Chaos::ClothAsset
{
	FClothCollection::FClothCollection(const TSharedRef<FManagedArrayCollection>& InManagedArrayCollection)
		: ManagedArrayCollection(InManagedArrayCollection)
	{
		using namespace UE::Chaos::ClothAsset::Private;

		// LODs Group
		PhysicsAssetPathName = ManagedArrayCollection->FindAttribute<FString>(ClothCollectionAttribute::PhysicsAssetPathName, ClothCollectionGroup::Lods);
		SkeletalMeshPathName = ManagedArrayCollection->FindAttribute<FString>(ClothCollectionAttribute::SkeletalMeshPathName, ClothCollectionGroup::Lods);

		// Solvers Group
		SolverGravity = ManagedArrayCollection->FindAttribute<FVector3f>(ClothCollectionAttribute::SolverGravity, ClothCollectionGroup::Solvers);
		SolverAirDamping = ManagedArrayCollection->FindAttribute<float>(ClothCollectionAttribute::SolverAirDamping, ClothCollectionGroup::Solvers);
		SolverSubSteps = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::SolverSubSteps, ClothCollectionGroup::Solvers);
		SolverTimeStep = ManagedArrayCollection->FindAttribute<float>(ClothCollectionAttribute::SolverTimeStep, ClothCollectionGroup::Solvers);

		// Seam Group
		SeamStitchStart = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::SeamStitchStart, ClothCollectionGroup::Seams);
		SeamStitchEnd = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::SeamStitchEnd, ClothCollectionGroup::Seams);

		// Seam Stitches Group
		SeamStitch2DEndIndices = ManagedArrayCollection->FindAttribute<FIntVector2>(ClothCollectionAttribute::SeamStitch2DEndIndices, ClothCollectionGroup::SeamStitches);
		SeamStitch3DIndex = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::SeamStitch3DIndex, ClothCollectionGroup::SeamStitches);

		// Sim Patterns Group
		SimVertices2DStart = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::SimVertices2DStart, ClothCollectionGroup::SimPatterns);
		SimVertices2DEnd = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::SimVertices2DEnd, ClothCollectionGroup::SimPatterns);
		SimFacesStart = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::SimFacesStart, ClothCollectionGroup::SimPatterns);
		SimFacesEnd = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::SimFacesEnd, ClothCollectionGroup::SimPatterns);
		SimPatternFabric = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::SimPatternFabric, ClothCollectionGroup::SimPatterns);

		// Render Patterns Group
		RenderVerticesStart = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::RenderVerticesStart, ClothCollectionGroup::RenderPatterns);
		RenderVerticesEnd = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::RenderVerticesEnd, ClothCollectionGroup::RenderPatterns);
		RenderFacesStart = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::RenderFacesStart, ClothCollectionGroup::RenderPatterns);
		RenderFacesEnd = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::RenderFacesEnd, ClothCollectionGroup::RenderPatterns);
		RenderDeformerNumInfluences = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::RenderDeformerNumInfluences, ClothCollectionGroup::RenderPatterns);
		RenderMaterialPathName = ManagedArrayCollection->FindAttribute<FString>(ClothCollectionAttribute::RenderMaterialPathName, ClothCollectionGroup::RenderPatterns);

		//~ Fabric Group
		FabricBendingStiffness = ManagedArrayCollection->FindAttribute<FVector3f>(ClothCollectionAttribute::FabricBendingStiffness, ClothCollectionGroup::Fabrics);
		FabricBucklingStiffness = ManagedArrayCollection->FindAttribute<FVector3f>(ClothCollectionAttribute::FabricBucklingStiffness, ClothCollectionGroup::Fabrics);
		FabricStretchStiffness = ManagedArrayCollection->FindAttribute<FVector3f>(ClothCollectionAttribute::FabricStretchStiffness, ClothCollectionGroup::Fabrics);
		FabricBucklingRatio = ManagedArrayCollection->FindAttribute<float>(ClothCollectionAttribute::FabricBucklingRatio, ClothCollectionGroup::Fabrics);
		FabricDensity = ManagedArrayCollection->FindAttribute<float>(ClothCollectionAttribute::FabricDensity, ClothCollectionGroup::Fabrics);
		FabricFriction = ManagedArrayCollection->FindAttribute<float>(ClothCollectionAttribute::FabricFriction, ClothCollectionGroup::Fabrics);
		FabricDamping = ManagedArrayCollection->FindAttribute<float>(ClothCollectionAttribute::FabricDamping, ClothCollectionGroup::Fabrics);
		FabricPressure = ManagedArrayCollection->FindAttribute<float>(ClothCollectionAttribute::FabricPressure, ClothCollectionGroup::Fabrics);
		FabricLayer = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::FabricLayer, ClothCollectionGroup::Fabrics);
		FabricCollisionThickness = ManagedArrayCollection->FindAttribute<float>(ClothCollectionAttribute::FabricCollisionThickness, ClothCollectionGroup::Fabrics);
		
		// Sim Faces Group
		SimIndices2D = ManagedArrayCollection->FindAttribute<FIntVector3>(ClothCollectionAttribute::SimIndices2D, ClothCollectionGroup::SimFaces);
		SimIndices3D = ManagedArrayCollection->FindAttribute<FIntVector3>(ClothCollectionAttribute::SimIndices3D, ClothCollectionGroup::SimFaces);

		// Sim Vertices 2D Group
		SimPosition2D = ManagedArrayCollection->FindAttribute<FVector2f>(ClothCollectionAttribute::SimPosition2D, ClothCollectionGroup::SimVertices2D);
		SimVertex3DLookup = ManagedArrayCollection->FindAttribute<int32>(ClothCollectionAttribute::SimVertex3DLookup, ClothCollectionGroup::SimVertices2D);

		// Sim Vertices 3D Group
		SimPosition3D = ManagedArrayCollection->FindAttribute<FVector3f>(ClothCollectionAttribute::SimPosition3D, ClothCollectionGroup::SimVertices3D);
		SimNormal = ManagedArrayCollection->FindAttribute<FVector3f>(ClothCollectionAttribute::SimNormal, ClothCollectionGroup::SimVertices3D);
		SimBoneIndices = ManagedArrayCollection->FindAttribute<TArray<int32>>(ClothCollectionAttribute::SimBoneIndices, ClothCollectionGroup::SimVertices3D);
		SimBoneWeights = ManagedArrayCollection->FindAttribute<TArray<float>>(ClothCollectionAttribute::SimBoneWeights, ClothCollectionGroup::SimVertices3D);
		TetherKinematicIndex = ManagedArrayCollection->FindAttribute<TArray<int32>>(ClothCollectionAttribute::TetherKinematicIndex, ClothCollectionGroup::SimVertices3D);
		TetherReferenceLength = ManagedArrayCollection->FindAttribute<TArray<float>>(ClothCollectionAttribute::TetherReferenceLength, ClothCollectionGroup::SimVertices3D);
		SimVertex2DLookup = ManagedArrayCollection->FindAttribute<TArray<int32>>(ClothCollectionAttribute::SimVertex2DLookup, ClothCollectionGroup::SimVertices3D);
		SeamStitchLookup = ManagedArrayCollection->FindAttribute<TArray<int32>>(ClothCollectionAttribute::SeamStitchLookup, ClothCollectionGroup::SimVertices3D);

		// Render Faces Group
		RenderIndices = ManagedArrayCollection->FindAttribute<FIntVector3>(ClothCollectionAttribute::RenderIndices, ClothCollectionGroup::RenderFaces);

		// Render Vertices Group
		RenderPosition = ManagedArrayCollection->FindAttribute<FVector3f>(ClothCollectionAttribute::RenderPosition, ClothCollectionGroup::RenderVertices);
		RenderNormal = ManagedArrayCollection->FindAttribute<FVector3f>(ClothCollectionAttribute::RenderNormal, ClothCollectionGroup::RenderVertices);
		RenderTangentU = ManagedArrayCollection->FindAttribute<FVector3f>(ClothCollectionAttribute::RenderTangentU, ClothCollectionGroup::RenderVertices);
		RenderTangentV = ManagedArrayCollection->FindAttribute<FVector3f>(ClothCollectionAttribute::RenderTangentV, ClothCollectionGroup::RenderVertices);
		RenderUVs = ManagedArrayCollection->FindAttribute<TArray<FVector2f>>(ClothCollectionAttribute::RenderUVs, ClothCollectionGroup::RenderVertices);
		RenderColor = ManagedArrayCollection->FindAttribute<FLinearColor>(ClothCollectionAttribute::RenderColor, ClothCollectionGroup::RenderVertices);
		RenderBoneIndices = ManagedArrayCollection->FindAttribute<TArray<int32>>(ClothCollectionAttribute::RenderBoneIndices, ClothCollectionGroup::RenderVertices);
		RenderBoneWeights = ManagedArrayCollection->FindAttribute<TArray<float>>(ClothCollectionAttribute::RenderBoneWeights, ClothCollectionGroup::RenderVertices);
		RenderDeformerPositionBaryCoordsAndDist = ManagedArrayCollection->FindAttribute<TArray<FVector4f>>(ClothCollectionAttribute::RenderDeformerPositionBaryCoordsAndDist, ClothCollectionGroup::RenderVertices);
		RenderDeformerNormalBaryCoordsAndDist = ManagedArrayCollection->FindAttribute<TArray<FVector4f>>(ClothCollectionAttribute::RenderDeformerNormalBaryCoordsAndDist, ClothCollectionGroup::RenderVertices);
		RenderDeformerTangentBaryCoordsAndDist = ManagedArrayCollection->FindAttribute<TArray<FVector4f>>(ClothCollectionAttribute::RenderDeformerTangentBaryCoordsAndDist, ClothCollectionGroup::RenderVertices);
		RenderDeformerSimIndices3D = ManagedArrayCollection->FindAttribute<TArray<FIntVector3>>(ClothCollectionAttribute::RenderDeformerSimIndices3D, ClothCollectionGroup::RenderVertices);
		RenderDeformerWeight = ManagedArrayCollection->FindAttribute<TArray<float>>(ClothCollectionAttribute::RenderDeformerWeight, ClothCollectionGroup::RenderVertices);
		RenderDeformerSkinningBlend = ManagedArrayCollection->FindAttribute<float>(ClothCollectionAttribute::RenderDeformerSkinningBlend, ClothCollectionGroup::RenderVertices);
	}

	bool FClothCollection::IsValid(EClothCollectionOptionalSchemas OptionalSchemas) const
	{
		return
			// LODs Group
			PhysicsAssetPathName &&
			SkeletalMeshPathName &&

			// Solvers Group
			(!EnumHasAnyFlags(OptionalSchemas, EClothCollectionOptionalSchemas::Solvers) ||
				(SolverGravity &&
				 SolverAirDamping &&
				 SolverSubSteps &&
				 SolverTimeStep )) &&

			// Seam Group
			SeamStitchStart &&
			SeamStitchEnd &&

			// Seam Stitches Group
			SeamStitch2DEndIndices &&
			SeamStitch3DIndex &&

			// Sim Patterns Group
			SimVertices2DStart &&
			SimVertices2DEnd &&
			SimFacesStart &&
			SimFacesEnd &&
			SimPatternFabric &&
		

			// Render Patterns Group
			RenderVerticesStart &&
			RenderVerticesEnd &&
			RenderFacesStart &&
			RenderFacesEnd &&
			RenderMaterialPathName &&
			(!EnumHasAnyFlags(OptionalSchemas, EClothCollectionOptionalSchemas::RenderDeformer) ||
				RenderDeformerNumInfluences) &&

			// Sim Faces Group
			SimIndices2D &&
			SimIndices3D &&

			// Fabrics Group
			(!EnumHasAnyFlags(OptionalSchemas, EClothCollectionOptionalSchemas::Fabrics) ||
				(FabricBendingStiffness &&
				FabricBucklingStiffness &&
				FabricStretchStiffness &&
				FabricBucklingRatio &&
				FabricDensity &&
				FabricFriction &&
				FabricDamping &&
				FabricPressure &&
				FabricLayer &&
				FabricCollisionThickness )) &&

			// Sim Vertices 2D Group
			SimPosition2D &&
			SimVertex3DLookup &&

			// Sim Vertices 3D Group
			SimPosition3D &&
			SimNormal &&
			SimBoneIndices &&
			SimBoneWeights &&
			TetherKinematicIndex &&
			TetherReferenceLength &&
			SimVertex2DLookup &&
			SeamStitchLookup &&

			// Render Faces Group
			RenderIndices &&

			// Render Vertices Group
			RenderPosition &&
			RenderNormal &&
			RenderTangentU &&
			RenderTangentV &&
			RenderUVs &&
			RenderColor &&
			RenderBoneIndices &&
			RenderBoneWeights &&
			(!EnumHasAnyFlags(OptionalSchemas, EClothCollectionOptionalSchemas::RenderDeformer) ||
				(RenderDeformerPositionBaryCoordsAndDist &&
				RenderDeformerNormalBaryCoordsAndDist &&
				RenderDeformerTangentBaryCoordsAndDist &&
				RenderDeformerSimIndices3D &&
				RenderDeformerWeight &&
				RenderDeformerSkinningBlend));
	}

	void FClothCollection::DefineSchema(EClothCollectionOptionalSchemas OptionalSchemas)
	{
		using namespace UE::Chaos::ClothAsset::Private;

		// Dependencies
		constexpr bool bSaved = true;
		constexpr bool bAllowCircularDependency = true;
		FManagedArrayCollection::FConstructionParameters SeamStitchesDependency(ClothCollectionGroup::SeamStitches, bSaved, bAllowCircularDependency);
		FManagedArrayCollection::FConstructionParameters RenderFacesDependency(ClothCollectionGroup::RenderFaces, bSaved, bAllowCircularDependency);
		FManagedArrayCollection::FConstructionParameters RenderVerticesDependency(ClothCollectionGroup::RenderVertices, bSaved, bAllowCircularDependency);
		FManagedArrayCollection::FConstructionParameters SimFacesDependency(ClothCollectionGroup::SimFaces, bSaved, bAllowCircularDependency);
		FManagedArrayCollection::FConstructionParameters SimFabricsDependency(ClothCollectionGroup::Fabrics, bSaved, bAllowCircularDependency);
		FManagedArrayCollection::FConstructionParameters SimVertices2DDependency(ClothCollectionGroup::SimVertices2D, bSaved, bAllowCircularDependency);
		FManagedArrayCollection::FConstructionParameters SimVertices3DDependency(ClothCollectionGroup::SimVertices3D, bSaved, bAllowCircularDependency);  // Any attribute with this dependency must handle welding and splitting in FCollectionClothSeamFacade

		// LODs Group
		PhysicsAssetPathName = &ManagedArrayCollection->AddAttribute<FString>(ClothCollectionAttribute::PhysicsAssetPathName, ClothCollectionGroup::Lods);
		SkeletalMeshPathName = &ManagedArrayCollection->AddAttribute<FString>(ClothCollectionAttribute::SkeletalMeshPathName, ClothCollectionGroup::Lods);

		// Solvers Group
		if (EnumHasAnyFlags(OptionalSchemas, EClothCollectionOptionalSchemas::Solvers))
		{
			SolverGravity = &ManagedArrayCollection->AddAttribute<FVector3f>(ClothCollectionAttribute::SolverGravity, ClothCollectionGroup::Solvers);
			SolverAirDamping = &ManagedArrayCollection->AddAttribute<float>(ClothCollectionAttribute::SolverAirDamping, ClothCollectionGroup::Solvers);
			SolverSubSteps = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::SolverSubSteps, ClothCollectionGroup::Solvers);
			SolverTimeStep = &ManagedArrayCollection->AddAttribute<float>(ClothCollectionAttribute::SolverTimeStep, ClothCollectionGroup::Solvers);
		}
		
		// Seams Group
		SeamStitchStart = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::SeamStitchStart, ClothCollectionGroup::Seams, SeamStitchesDependency);
		SeamStitchEnd = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::SeamStitchEnd, ClothCollectionGroup::Seams, SeamStitchesDependency);

		// Seam Stitches Group
		SeamStitch2DEndIndices = &ManagedArrayCollection->AddAttribute<FIntVector2>(ClothCollectionAttribute::SeamStitch2DEndIndices, ClothCollectionGroup::SeamStitches, SimVertices2DDependency);
		SeamStitch3DIndex = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::SeamStitch3DIndex, ClothCollectionGroup::SeamStitches, SimVertices3DDependency);

		// Sim Patterns Group
		SimVertices2DStart = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::SimVertices2DStart, ClothCollectionGroup::SimPatterns, SimVertices2DDependency);
		SimVertices2DEnd = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::SimVertices2DEnd, ClothCollectionGroup::SimPatterns, SimVertices2DDependency);
		SimFacesStart = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::SimFacesStart, ClothCollectionGroup::SimPatterns, SimFacesDependency);
		SimFacesEnd = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::SimFacesEnd, ClothCollectionGroup::SimPatterns, SimFacesDependency);
		SimPatternFabric = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::SimPatternFabric, ClothCollectionGroup::SimPatterns, SimFabricsDependency);

		// Render Patterns Group
		RenderVerticesStart = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::RenderVerticesStart, ClothCollectionGroup::RenderPatterns, RenderVerticesDependency);
		RenderVerticesEnd = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::RenderVerticesEnd, ClothCollectionGroup::RenderPatterns, RenderVerticesDependency);
		RenderFacesStart = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::RenderFacesStart, ClothCollectionGroup::RenderPatterns, RenderFacesDependency);
		RenderFacesEnd = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::RenderFacesEnd, ClothCollectionGroup::RenderPatterns, RenderFacesDependency);
		RenderMaterialPathName = &ManagedArrayCollection->AddAttribute<FString>(ClothCollectionAttribute::RenderMaterialPathName, ClothCollectionGroup::RenderPatterns);

		//~ Fabric Group
		if (EnumHasAnyFlags(OptionalSchemas, EClothCollectionOptionalSchemas::Fabrics))
		{
			FabricBendingStiffness = &ManagedArrayCollection->AddAttribute<FVector3f>(ClothCollectionAttribute::FabricBendingStiffness, ClothCollectionGroup::Fabrics);
			FabricBucklingStiffness = &ManagedArrayCollection->AddAttribute<FVector3f>(ClothCollectionAttribute::FabricBucklingStiffness, ClothCollectionGroup::Fabrics);
			FabricStretchStiffness = &ManagedArrayCollection->AddAttribute<FVector3f>(ClothCollectionAttribute::FabricStretchStiffness, ClothCollectionGroup::Fabrics);
			FabricBucklingRatio = &ManagedArrayCollection->AddAttribute<float>(ClothCollectionAttribute::FabricBucklingRatio, ClothCollectionGroup::Fabrics);
			FabricDensity = &ManagedArrayCollection->AddAttribute<float>(ClothCollectionAttribute::FabricDensity, ClothCollectionGroup::Fabrics);
			FabricFriction = &ManagedArrayCollection->AddAttribute<float>(ClothCollectionAttribute::FabricFriction, ClothCollectionGroup::Fabrics);
			FabricDamping = &ManagedArrayCollection->AddAttribute<float>(ClothCollectionAttribute::FabricDamping, ClothCollectionGroup::Fabrics);
			FabricPressure = &ManagedArrayCollection->AddAttribute<float>(ClothCollectionAttribute::FabricPressure, ClothCollectionGroup::Fabrics);
			FabricLayer = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::FabricLayer, ClothCollectionGroup::Fabrics);
			FabricCollisionThickness = &ManagedArrayCollection->AddAttribute<float>(ClothCollectionAttribute::FabricCollisionThickness, ClothCollectionGroup::Fabrics);
		}
		
		// Sim Faces Group
		SimIndices2D = &ManagedArrayCollection->AddAttribute<FIntVector3>(ClothCollectionAttribute::SimIndices2D, ClothCollectionGroup::SimFaces, SimVertices2DDependency);
		SimIndices3D = &ManagedArrayCollection->AddAttribute<FIntVector3>(ClothCollectionAttribute::SimIndices3D, ClothCollectionGroup::SimFaces, SimVertices3DDependency);

		// Sim Vertices 2D Group
		SimPosition2D = &ManagedArrayCollection->AddAttribute<FVector2f>(ClothCollectionAttribute::SimPosition2D, ClothCollectionGroup::SimVertices2D);
		SimVertex3DLookup = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::SimVertex3DLookup, ClothCollectionGroup::SimVertices2D, SimVertices3DDependency);

		// Sim Vertices 3D Group
		SimPosition3D = &ManagedArrayCollection->AddAttribute<FVector3f>(ClothCollectionAttribute::SimPosition3D, ClothCollectionGroup::SimVertices3D);
		SimNormal = &ManagedArrayCollection->AddAttribute<FVector3f>(ClothCollectionAttribute::SimNormal, ClothCollectionGroup::SimVertices3D);
		SimBoneIndices = &ManagedArrayCollection->AddAttribute<TArray<int32>>(ClothCollectionAttribute::SimBoneIndices, ClothCollectionGroup::SimVertices3D);
		SimBoneWeights = &ManagedArrayCollection->AddAttribute<TArray<float>>(ClothCollectionAttribute::SimBoneWeights, ClothCollectionGroup::SimVertices3D);
		TetherKinematicIndex = &ManagedArrayCollection->AddAttribute<TArray<int32>>(ClothCollectionAttribute::TetherKinematicIndex, ClothCollectionGroup::SimVertices3D, SimVertices3DDependency);
		TetherReferenceLength = &ManagedArrayCollection->AddAttribute<TArray<float>>(ClothCollectionAttribute::TetherReferenceLength, ClothCollectionGroup::SimVertices3D);
		SimVertex2DLookup = &ManagedArrayCollection->AddAttribute<TArray<int32>>(ClothCollectionAttribute::SimVertex2DLookup, ClothCollectionGroup::SimVertices3D, SimVertices2DDependency);
		SeamStitchLookup = &ManagedArrayCollection->AddAttribute<TArray<int32>>(ClothCollectionAttribute::SeamStitchLookup, ClothCollectionGroup::SimVertices3D, SeamStitchesDependency);

		// Render Faces Group
		RenderIndices = &ManagedArrayCollection->AddAttribute<FIntVector3>(ClothCollectionAttribute::RenderIndices, ClothCollectionGroup::RenderFaces, RenderVerticesDependency);

		// Render Vertices Group
		RenderPosition = &ManagedArrayCollection->AddAttribute<FVector3f>(ClothCollectionAttribute::RenderPosition, ClothCollectionGroup::RenderVertices);
		RenderNormal = &ManagedArrayCollection->AddAttribute<FVector3f>(ClothCollectionAttribute::RenderNormal, ClothCollectionGroup::RenderVertices);
		RenderTangentU = &ManagedArrayCollection->AddAttribute<FVector3f>(ClothCollectionAttribute::RenderTangentU, ClothCollectionGroup::RenderVertices);
		RenderTangentV = &ManagedArrayCollection->AddAttribute<FVector3f>(ClothCollectionAttribute::RenderTangentV, ClothCollectionGroup::RenderVertices);
		RenderUVs = &ManagedArrayCollection->AddAttribute<TArray<FVector2f>>(ClothCollectionAttribute::RenderUVs, ClothCollectionGroup::RenderVertices);
		RenderColor = &ManagedArrayCollection->AddAttribute<FLinearColor>(ClothCollectionAttribute::RenderColor, ClothCollectionGroup::RenderVertices);
		RenderBoneIndices = &ManagedArrayCollection->AddAttribute<TArray<int32>>(ClothCollectionAttribute::RenderBoneIndices, ClothCollectionGroup::RenderVertices);
		RenderBoneWeights = &ManagedArrayCollection->AddAttribute<TArray<float>>(ClothCollectionAttribute::RenderBoneWeights, ClothCollectionGroup::RenderVertices);

		if (EnumHasAnyFlags(OptionalSchemas, EClothCollectionOptionalSchemas::RenderDeformer))
		{
			// Render Patterns Group
			RenderDeformerNumInfluences = &ManagedArrayCollection->AddAttribute<int32>(ClothCollectionAttribute::RenderDeformerNumInfluences, ClothCollectionGroup::RenderPatterns);

			// Render Vertices Group
			RenderDeformerPositionBaryCoordsAndDist = &ManagedArrayCollection->AddAttribute<TArray<FVector4f>>(ClothCollectionAttribute::RenderDeformerPositionBaryCoordsAndDist, ClothCollectionGroup::RenderVertices);
			RenderDeformerNormalBaryCoordsAndDist = &ManagedArrayCollection->AddAttribute<TArray<FVector4f>>(ClothCollectionAttribute::RenderDeformerNormalBaryCoordsAndDist, ClothCollectionGroup::RenderVertices);
			RenderDeformerTangentBaryCoordsAndDist = &ManagedArrayCollection->AddAttribute<TArray<FVector4f>>(ClothCollectionAttribute::RenderDeformerTangentBaryCoordsAndDist, ClothCollectionGroup::RenderVertices);
			RenderDeformerSimIndices3D = &ManagedArrayCollection->AddAttribute<TArray<FIntVector3>>(ClothCollectionAttribute::RenderDeformerSimIndices3D, ClothCollectionGroup::RenderVertices, SimVertices3DDependency);
			RenderDeformerWeight = &ManagedArrayCollection->AddAttribute<TArray<float>>(ClothCollectionAttribute::RenderDeformerWeight, ClothCollectionGroup::RenderVertices);
			RenderDeformerSkinningBlend = &ManagedArrayCollection->AddAttribute<float>(ClothCollectionAttribute::RenderDeformerSkinningBlend, ClothCollectionGroup::RenderVertices);
		}
	}

	int32 FClothCollection::GetNumElements(const FName& GroupName) const
	{
		return ManagedArrayCollection->NumElements(GroupName);
	}

	int32 FClothCollection::GetNumElements(const TManagedArray<int32>* StartArray, const TManagedArray<int32>* EndArray, int32 ArrayIndex) const
	{
		if (StartArray && StartArray->GetConstArray().IsValidIndex(ArrayIndex) &&
			EndArray && EndArray->GetConstArray().IsValidIndex(ArrayIndex))
		{
			const int32 Start = (*StartArray)[ArrayIndex];
			const int32 End = (*EndArray)[ArrayIndex];
			if (Start != INDEX_NONE && End != INDEX_NONE)
			{
				return End - Start + 1;
			}
			checkf(Start == End, TEXT("Only one boundary of the range is set to INDEX_NONE, when both should."));
		}
		return 0;
	}

	void FClothCollection::SetNumElements(int32 InNumElements, const FName& GroupName)
	{
		check(IsValid());
		check(InNumElements >= 0);
		
		const int32 NumElements = ManagedArrayCollection->NumElements(GroupName);

		if (const int32 Delta = InNumElements - NumElements)
		{
			if (Delta > 0)
			{
				ManagedArrayCollection->AddElements(Delta, GroupName);
			}
			else
			{
				ManagedArrayCollection->RemoveElements(GroupName, -Delta, InNumElements);
			}
		}
	}

	int32 FClothCollection::SetNumElements(int32 InNumElements, const FName& GroupName, TManagedArray<int32>* StartArray, TManagedArray<int32>* EndArray, int32 ArrayIndex)
	{
		check(IsValid());
		check(InNumElements >= 0);

		check(StartArray && StartArray->GetConstArray().IsValidIndex(ArrayIndex));
		check(EndArray && EndArray->GetConstArray().IsValidIndex(ArrayIndex));

		int32& Start = (*StartArray)[ArrayIndex];
		int32& End = (*EndArray)[ArrayIndex];
		check(Start != INDEX_NONE || End == INDEX_NONE);  // Best to avoid situations where only one boundary of the range is set to INDEX_NONE

		const int32 NumElements = (Start == INDEX_NONE) ? 0 : End - Start + 1;

		if (const int32 Delta = InNumElements - NumElements)
		{
			if (Delta > 0)
			{
				// Find a previous valid index range to insert after when the range is empty
				auto ComputeEnd = [&EndArray, ArrayIndex]()->int32
				{
					for (int32 Index = ArrayIndex; Index >= 0; --Index)
					{
						if ((*EndArray)[Index] != INDEX_NONE)
						{
							return (*EndArray)[Index];
						}
					}
					return INDEX_NONE;
				};

				// Grow the array
				const int32 Position = ComputeEnd() + 1;
				ManagedArrayCollection->InsertElements(Delta, Position, GroupName);

				// Update Start/End
				if (!NumElements)
				{
					Start = Position;
				}
				End = Start + InNumElements - 1;
			}
			else
			{
				// Shrink the array
				const int32 Position = Start + InNumElements;
				ManagedArrayCollection->RemoveElements(GroupName, -Delta, Position);

				// Update Start/End
				if (InNumElements)
				{
					End = Position - 1;
				}
				else
				{
					End = Start = INDEX_NONE;  // It is important to set the start & end to INDEX_NONE so that they never get automatically re-indexed by the managed array collection
				}
			}
		}
		return Start;
	}

	void FClothCollection::RemoveElements(const FName& Group, const TArray<int32>& SortedDeletionList)
	{
		ManagedArrayCollection->RemoveElements(Group, SortedDeletionList);
	}

	void FClothCollection::RemoveElements(const FName& GroupName, const TArray<int32>& SortedDeletionList, TManagedArray<int32>* StartArray, TManagedArray<int32>* EndArray, int32 ArrayIndex)
	{
		if (SortedDeletionList.IsEmpty())
		{
			return;
		}

		check(IsValid());

		check(StartArray && StartArray->GetConstArray().IsValidIndex(ArrayIndex));
		check(EndArray && EndArray->GetConstArray().IsValidIndex(ArrayIndex));

		int32& Start = (*StartArray)[ArrayIndex];
		int32& End = (*EndArray)[ArrayIndex];
		check(Start != INDEX_NONE && End != INDEX_NONE);

		const int32 OrigStart = Start;
		const int32 OrigNumElements = End - Start + 1;

		check(SortedDeletionList[0] >= Start);
		check(SortedDeletionList.Last() <= End);
		check(OrigNumElements >= SortedDeletionList.Num());

		ManagedArrayCollection->RemoveElements(GroupName, SortedDeletionList);

		if (SortedDeletionList.Num() == OrigNumElements)
		{
			Start = End = INDEX_NONE;
		}
		else
		{
			const int32 NewNumElements = OrigNumElements - SortedDeletionList.Num();
			const int32 NewEnd = OrigStart + NewNumElements - 1;
			check(Start == OrigStart || Start == INDEX_NONE);
			check(End == NewEnd || End == INDEX_NONE);
			Start = OrigStart;
			End = NewEnd;
		}
	}

	int32 FClothCollection::GetElementsOffset(const TManagedArray<int32>* StartArray, int32 BaseElementIndex, int32 ElementIndex)
	{
		while ((*StartArray)[BaseElementIndex] == INDEX_NONE && BaseElementIndex < ElementIndex)
		{
			++BaseElementIndex;
		}
		return (*StartArray)[ElementIndex] - (*StartArray)[BaseElementIndex];
	}

	int32 FClothCollection::GetArrayIndexForContainedElement(
		const TManagedArray<int32>* StartArray,
		const TManagedArray<int32>* EndArray,
		int32 ElementIndex)
	{
		for (int32 ArrayIndex = 0; ArrayIndex < StartArray->Num(); ++ArrayIndex)
		{
			if (ElementIndex >= (*StartArray)[ArrayIndex] && ElementIndex <= (*EndArray)[ArrayIndex])
			{
				return ArrayIndex;
			}
		}
		return INDEX_NONE;
	}

	int32 FClothCollection::GetNumSubElements(
		const TManagedArray<int32>* StartArray,
		const TManagedArray<int32>* EndArray,
		const TManagedArray<int32>* StartSubArray,
		const TManagedArray<int32>* EndSubArray,
		int32 ArrayIndex)
	{
		const TTuple<int32, int32> StartEnd = GetSubElementsStartEnd(StartArray, EndArray, StartSubArray, EndSubArray, ArrayIndex);
		const int32 Start = StartEnd.Get<0>();
		const int32 End = StartEnd.Get<1>();
		if (Start != INDEX_NONE && End != INDEX_NONE)
		{
			return End - Start + 1;
		}
		checkf(Start == End, TEXT("Only one boundary of the range is set to INDEX_NONE, when both should."));
		return 0;
	}

	template<bool bStart, bool bEnd>
	TTuple<int32, int32> FClothCollection::GetSubElementsStartEnd(
		const TManagedArray<int32>* StartArray,
		const TManagedArray<int32>* EndArray,
		const TManagedArray<int32>* StartSubArray,
		const TManagedArray<int32>* EndSubArray,
		int32 ArrayIndex)
	{
		int32 Start = INDEX_NONE;  // Find Start and End indices for the entire LOD minding empty patterns on the way
		int32 End = INDEX_NONE;

		if (StartArray && StartArray->GetConstArray().IsValidIndex(ArrayIndex) &&
			EndArray && EndArray->GetConstArray().IsValidIndex(ArrayIndex))
		{
			const int32 SubStart = (*StartArray)[ArrayIndex];
			const int32 SubEnd = (*EndArray)[ArrayIndex];

			if (SubStart != INDEX_NONE && SubEnd != INDEX_NONE)
			{
				for (int32 SubIndex = SubStart; SubIndex <= SubEnd; ++SubIndex)
				{
					if (bStart && (*StartSubArray)[SubIndex] != INDEX_NONE)
					{
						Start = (Start == INDEX_NONE) ? (*StartSubArray)[SubIndex] : FMath::Min(Start, (*StartSubArray)[SubIndex]);
					}
					if (bEnd && (*EndSubArray)[SubIndex] != INDEX_NONE)
					{
						End = (End == INDEX_NONE) ? (*EndSubArray)[SubIndex] : FMath::Max(End, (*EndSubArray)[SubIndex]);
					}
				}
			}
			else
			{
				checkf(SubStart == SubEnd, TEXT("Only one boundary of the range is set to INDEX_NONE, when both should."));
			}
		}
		return TTuple<int32, int32>(Start, End);
	}
	template CHAOSCLOTHASSET_API TTuple<int32, int32> FClothCollection::GetSubElementsStartEnd<true, false>(const TManagedArray<int32>* StartArray, const TManagedArray<int32>* EndArray, const TManagedArray<int32>* StartSubArray, const TManagedArray<int32>* EndSubArray, int32 ArrayIndex);
	template CHAOSCLOTHASSET_API TTuple<int32, int32> FClothCollection::GetSubElementsStartEnd<false, true>(const TManagedArray<int32>* StartArray, const TManagedArray<int32>* EndArray, const TManagedArray<int32>* StartSubArray, const TManagedArray<int32>* EndSubArray, int32 ArrayIndex);
	template CHAOSCLOTHASSET_API TTuple<int32, int32> FClothCollection::GetSubElementsStartEnd<true, true>(const TManagedArray<int32>* StartArray, const TManagedArray<int32>* EndArray, const TManagedArray<int32>* StartSubArray, const TManagedArray<int32>* EndSubArray, int32 ArrayIndex);

	template<typename T, typename TEnableIf<TIsUserAttributeType<T>::Value, int>::type>
	TArray<FName> FClothCollection::GetUserDefinedAttributeNames(const FName& GroupName) const
	{
		using namespace UE::Chaos::ClothAsset::Private;

		TArray<FName> UserDefinedAttributeNames;

		if (const TArray<FName>* const FixedAttributeNames = FixedAttributeNamesMap.Find(GroupName))  // Also checks that the group name is a recognized group name
		{
			const TArray<FName> AttributeNames = ManagedArrayCollection->AttributeNames(GroupName);

			for (const FName& AttributeName : AttributeNames)
			{
				if (!FixedAttributeNames->Contains(AttributeName) && ManagedArrayCollection->FindAttributeTyped<T>(AttributeName, GroupName))
				{
					UserDefinedAttributeNames.Add(AttributeName);
				}
			}
		}

		return UserDefinedAttributeNames;
	}
	template CHAOSCLOTHASSET_API TArray<FName> FClothCollection::GetUserDefinedAttributeNames<bool>(const FName& GroupName) const;
	template CHAOSCLOTHASSET_API TArray<FName> FClothCollection::GetUserDefinedAttributeNames<int32>(const FName& GroupName) const;
	template CHAOSCLOTHASSET_API TArray<FName> FClothCollection::GetUserDefinedAttributeNames<float>(const FName& GroupName) const;
	template CHAOSCLOTHASSET_API TArray<FName> FClothCollection::GetUserDefinedAttributeNames<FVector3f>(const FName& GroupName) const;

	template<typename T, typename TEnableIf<TIsUserAttributeType<T>::Value, int>::type>
	TManagedArray<T>* FClothCollection::FindOrAddUserDefinedAttribute(const FName& Name, const FName& GroupName)
	{
		using namespace UE::Chaos::ClothAsset::Private;
		if (const TArray<FName>* const FixedAttributeNames = FixedAttributeNamesMap.Find(GroupName))
		{
			// Only allow adding to know groups. Do not allow adding a name reserved as a fixed attribute.
			if (!FixedAttributeNames->Contains(Name))
			{
				return ManagedArrayCollection->FindOrAddAttributeTyped<T>(Name, GroupName);
			}
		}
		return nullptr;
	}
	template CHAOSCLOTHASSET_API TManagedArray<bool>* FClothCollection::FindOrAddUserDefinedAttribute<bool>(const FName& Name, const FName& GroupName);
	template CHAOSCLOTHASSET_API TManagedArray<int32>* FClothCollection::FindOrAddUserDefinedAttribute<int32>(const FName& Name, const FName& GroupName);
	template CHAOSCLOTHASSET_API TManagedArray<float>* FClothCollection::FindOrAddUserDefinedAttribute<float>(const FName& Name, const FName& GroupName);
	template CHAOSCLOTHASSET_API TManagedArray<FVector3f>* FClothCollection::FindOrAddUserDefinedAttribute<FVector3f>(const FName& Name, const FName& GroupName);

	void FClothCollection::RemoveUserDefinedAttribute(const FName& Name, const FName& GroupName)
	{
		using namespace UE::Chaos::ClothAsset::Private;
		if (const TArray<FName>* const FixedAttributeNames = FixedAttributeNamesMap.Find(GroupName))
		{
			// User defined attributes are only allowed in known group names. Do not allow removing fixed attributes through this method.
			if (!FixedAttributeNames->Contains(Name))
			{
				ManagedArrayCollection->RemoveAttribute(Name, GroupName);
			}
		}
	}

	template<typename T, typename TEnableIf<TIsUserAttributeType<T>::Value, int>::type>
	bool FClothCollection::HasUserDefinedAttribute(const FName& Name, const FName& GroupName) const
	{
		using namespace UE::Chaos::ClothAsset::Private;
		if (const TArray<FName>* const FixedAttributeNames = FixedAttributeNamesMap.Find(GroupName))
		{
			// User defined attributes are only allowed in known group names.
			if (!FixedAttributeNames->Contains(Name))
			{
				return ManagedArrayCollection->FindAttributeTyped<T>(Name, GroupName) != nullptr;
			}
		}
		return false;
	}
	template CHAOSCLOTHASSET_API bool FClothCollection::HasUserDefinedAttribute<bool>(const FName& Name, const FName& GroupName) const;
	template CHAOSCLOTHASSET_API bool FClothCollection::HasUserDefinedAttribute<int32>(const FName& Name, const FName& GroupName) const;
	template CHAOSCLOTHASSET_API bool FClothCollection::HasUserDefinedAttribute<float>(const FName& Name, const FName& GroupName) const;
	template CHAOSCLOTHASSET_API bool FClothCollection::HasUserDefinedAttribute<FVector3f>(const FName& Name, const FName& GroupName) const;

	template<typename T, typename TEnableIf<TIsUserAttributeType<T>::Value, int>::type>
	const TManagedArray<T>* FClothCollection::GetUserDefinedAttribute(const FName& Name, const FName& GroupName) const
	{
		using namespace UE::Chaos::ClothAsset::Private;
		if (const TArray<FName>* const FixedAttributeNames = FixedAttributeNamesMap.Find(GroupName))
		{
			// User defined attributes are only allowed in known group names.
			if (!FixedAttributeNames->Contains(Name))
			{
				return ManagedArrayCollection->FindAttributeTyped<T>(Name, GroupName);
			}
		}
		return nullptr;
	}
	template CHAOSCLOTHASSET_API const TManagedArray<bool>* FClothCollection::GetUserDefinedAttribute<bool>(const FName& Name, const FName& GroupName) const;
	template CHAOSCLOTHASSET_API const TManagedArray<int32>* FClothCollection::GetUserDefinedAttribute<int32>(const FName& Name, const FName& GroupName) const;
	template CHAOSCLOTHASSET_API const TManagedArray<float>* FClothCollection::GetUserDefinedAttribute<float>(const FName& Name, const FName& GroupName) const;
	template CHAOSCLOTHASSET_API const TManagedArray<FVector3f>* FClothCollection::GetUserDefinedAttribute<FVector3f>(const FName& Name, const FName& GroupName) const;

	template<typename T, typename TEnableIf<TIsUserAttributeType<T>::Value, int>::type>
	TManagedArray<T>* FClothCollection::GetUserDefinedAttribute(const FName& Name, const FName& GroupName)
	{
		using namespace UE::Chaos::ClothAsset::Private;
		if (const TArray<FName>* const FixedAttributeNames = FixedAttributeNamesMap.Find(GroupName))
		{
			// User defined attributes are only allowed in known group names.
			if (!FixedAttributeNames->Contains(Name))
			{
				return ManagedArrayCollection->FindAttributeTyped<T>(Name, GroupName);
			}
		}
		return nullptr;
	}
	template CHAOSCLOTHASSET_API TManagedArray<bool>* FClothCollection::GetUserDefinedAttribute<bool>(const FName& Name, const FName& GroupName);
	template CHAOSCLOTHASSET_API TManagedArray<int32>* FClothCollection::GetUserDefinedAttribute<int32>(const FName& Name, const FName& GroupName);
	template CHAOSCLOTHASSET_API TManagedArray<float>* FClothCollection::GetUserDefinedAttribute<float>(const FName& Name, const FName& GroupName);
	template CHAOSCLOTHASSET_API TManagedArray<FVector3f>* FClothCollection::GetUserDefinedAttribute<FVector3f>(const FName& Name, const FName& GroupName);

	bool FClothCollection::IsValidClothCollectionGroupName(const FName& GroupName)
	{
		using namespace UE::Chaos::ClothAsset::Private;
		return FixedAttributeNamesMap.Contains(GroupName);
	}

	bool FClothCollection::IsValidUserDefinedAttributeName(const FName& Name, const FName& GroupName)
	{
		using namespace UE::Chaos::ClothAsset::Private;
		if (const TArray<FName>* const FixedAttributeNames = FixedAttributeNamesMap.Find(GroupName))
		{
			// User defined attributes are only allowed in known group names and cannot be reserved by FixedAttributeNames.
			if (!FixedAttributeNames->Contains(Name))
			{
				return true;
			}
		}
		return false;
	}
} // End namespace UE::Chaos::ClothAsset
