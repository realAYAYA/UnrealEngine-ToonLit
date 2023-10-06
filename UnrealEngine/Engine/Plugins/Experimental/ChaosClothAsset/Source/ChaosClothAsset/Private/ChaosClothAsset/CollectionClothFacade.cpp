// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/ClothCollection.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "Chaos/ChaosArchive.h"
#include "UObject/UE5MainStreamObjectVersion.h"

namespace UE::Chaos::ClothAsset
{
	FCollectionClothConstFacade::FCollectionClothConstFacade(const TSharedRef<const FManagedArrayCollection>& ManagedArrayCollection)
		: ClothCollection(MakeShared<const FClothCollection>(ConstCastSharedRef<FManagedArrayCollection>(ManagedArrayCollection)))
	{
	}

	FCollectionClothConstFacade::FCollectionClothConstFacade(const TSharedRef<const FClothCollection>& ClothCollection)
		:ClothCollection(ClothCollection)
	{
	}

	bool FCollectionClothConstFacade::IsValid() const
	{
		return ClothCollection->IsValid() && ClothCollection->GetNumElements(FClothCollection::LodsGroup) == 1;
	}

	const FString& FCollectionClothConstFacade::GetPhysicsAssetPathName() const
	{
		static const FString EmptyString;
		return ClothCollection->GetPhysicsAssetPathName() && ClothCollection->GetNumElements(FClothCollection::LodsGroup) > 0 ? (*ClothCollection->GetPhysicsAssetPathName())[0] : EmptyString;
	}

	const FString& FCollectionClothConstFacade::GetSkeletalMeshPathName() const
	{
		static const FString EmptyString;
		return ClothCollection->GetSkeletalMeshPathName() && ClothCollection->GetNumElements(FClothCollection::LodsGroup) > 0 ? (*ClothCollection->GetSkeletalMeshPathName())[0] : EmptyString;
	}

	int32 FCollectionClothConstFacade::GetNumSimVertices2D() const
	{
		return ClothCollection->GetNumElements(FClothCollection::SimVertices2DGroup);
	}

	TConstArrayView<FVector2f> FCollectionClothConstFacade::GetSimPosition2D() const
	{
		return ClothCollection->GetElements(ClothCollection->GetSimPosition2D());
	}

	TConstArrayView<int32> FCollectionClothConstFacade::GetSimVertex3DLookup() const
	{
		return ClothCollection->GetElements(ClothCollection->GetSimVertex3DLookup());
	}

	int32 FCollectionClothConstFacade::GetNumSimVertices3D() const
	{
		return ClothCollection->GetNumElements(FClothCollection::SimVertices3DGroup);
	}

	TConstArrayView<FVector3f> FCollectionClothConstFacade::GetSimPosition3D() const
	{
		return ClothCollection->GetElements(ClothCollection->GetSimPosition3D());
	}

	TConstArrayView<FVector3f> FCollectionClothConstFacade::GetSimNormal() const
	{
		return ClothCollection->GetElements(ClothCollection->GetSimNormal());
	}

	TConstArrayView<TArray<int32>> FCollectionClothConstFacade::GetSimBoneIndices() const
	{
		return ClothCollection->GetElements(ClothCollection->GetSimBoneIndices());
	}

	TConstArrayView<TArray<float>> FCollectionClothConstFacade::GetSimBoneWeights() const
	{
		return ClothCollection->GetElements(ClothCollection->GetSimBoneWeights());
	}

	TConstArrayView<TArray<int32>> FCollectionClothConstFacade::GetTetherKinematicIndex() const
	{
		return ClothCollection->GetElements(ClothCollection->GetTetherKinematicIndex());
	}

	TConstArrayView<TArray<float>> FCollectionClothConstFacade::GetTetherReferenceLength() const
	{
		return ClothCollection->GetElements(ClothCollection->GetTetherReferenceLength());
	}

	TConstArrayView<TArray<int32>> FCollectionClothConstFacade::GetSimVertex2DLookup() const
	{
		return ClothCollection->GetElements(ClothCollection->GetSimVertex2DLookup());
	}

	TConstArrayView<TArray<int32>> FCollectionClothConstFacade::GetSeamStitchLookup() const
	{
		return ClothCollection->GetElements(ClothCollection->GetSeamStitchLookup());
	}

	int32 FCollectionClothConstFacade::GetNumSimFaces() const
	{
		return ClothCollection->GetNumElements(FClothCollection::SimFacesGroup);
	}

	TConstArrayView<FIntVector3> FCollectionClothConstFacade::GetSimIndices2D() const
	{
		return ClothCollection->GetElements(ClothCollection->GetSimIndices2D());
	}

	TConstArrayView<FIntVector3> FCollectionClothConstFacade::GetSimIndices3D() const
	{
		return ClothCollection->GetElements(ClothCollection->GetSimIndices3D());
	}

	int32 FCollectionClothConstFacade::GetNumSimPatterns() const
	{
		return ClothCollection->GetNumElements(FClothCollection::SimPatternsGroup);
	}

	FCollectionClothSimPatternConstFacade FCollectionClothConstFacade::GetSimPattern(int32 PatternIndex) const
	{
		return FCollectionClothSimPatternConstFacade(ClothCollection, PatternIndex);
	}

	int32 FCollectionClothConstFacade::FindSimPatternByVertex2D(int32 Vertex2DIndex) const
	{
		return ClothCollection->GetArrayIndexForContainedElement(ClothCollection->GetSimVertices2DStart(), ClothCollection->GetSimVertices2DEnd(), Vertex2DIndex);
	}

	int32 FCollectionClothConstFacade::FindSimPatternByFaceIndex(int32 FaceIndex) const
	{
		return ClothCollection->GetArrayIndexForContainedElement(ClothCollection->GetSimFacesStart(), ClothCollection->GetSimFacesEnd(), FaceIndex);
	}

	int32 FCollectionClothConstFacade::GetNumRenderPatterns() const
	{
		return ClothCollection->GetNumElements(FClothCollection::RenderPatternsGroup);
	}

	FCollectionClothRenderPatternConstFacade FCollectionClothConstFacade::GetRenderPattern(int32 PatternIndex) const
	{
		return FCollectionClothRenderPatternConstFacade(ClothCollection, PatternIndex);
	}

	TConstArrayView<FString> FCollectionClothConstFacade::GetRenderMaterialPathName() const
	{
		return ClothCollection->GetElements(ClothCollection->GetRenderMaterialPathName());
	}

	int32 FCollectionClothConstFacade::FindRenderPatternByVertex(int32 VertexIndex) const
	{
		return ClothCollection->GetArrayIndexForContainedElement(ClothCollection->GetRenderVerticesStart(), ClothCollection->GetRenderVerticesEnd(), VertexIndex);
	}

	int32 FCollectionClothConstFacade::FindRenderPatternByFaceIndex(int32 FaceIndex) const
	{
		return ClothCollection->GetArrayIndexForContainedElement(ClothCollection->GetRenderFacesStart(), ClothCollection->GetRenderFacesEnd(), FaceIndex);
	}

	int32 FCollectionClothConstFacade::GetNumSeams() const
	{
		return ClothCollection->GetNumElements(FClothCollection::SeamsGroup);
	}

	FCollectionClothSeamConstFacade FCollectionClothConstFacade::GetSeam(int32 SeamIndex) const
	{
		return FCollectionClothSeamConstFacade(ClothCollection, SeamIndex);
	}

	int32 FCollectionClothConstFacade::GetNumRenderVertices() const
	{
		return ClothCollection->GetNumElements(FClothCollection::RenderVerticesGroup);
	}

	TConstArrayView<FVector3f> FCollectionClothConstFacade::GetRenderPosition() const
	{
		return ClothCollection->GetElements(ClothCollection->GetRenderPosition());
	}

	TConstArrayView<FVector3f> FCollectionClothConstFacade::GetRenderNormal() const
	{
		return ClothCollection->GetElements(ClothCollection->GetRenderNormal());
	}

	TConstArrayView<FVector3f> FCollectionClothConstFacade::GetRenderTangentU() const
	{
		return ClothCollection->GetElements(ClothCollection->GetRenderTangentU());
	}

	TConstArrayView<FVector3f> FCollectionClothConstFacade::GetRenderTangentV() const
	{
		return ClothCollection->GetElements(ClothCollection->GetRenderTangentV());
	}

	TConstArrayView<TArray<FVector2f>> FCollectionClothConstFacade::GetRenderUVs() const
	{
		return ClothCollection->GetElements(ClothCollection->GetRenderUVs());
	}

	TConstArrayView<FLinearColor> FCollectionClothConstFacade::GetRenderColor() const
	{
		return ClothCollection->GetElements(ClothCollection->GetRenderColor());
	}

	TConstArrayView<TArray<int32>> FCollectionClothConstFacade::GetRenderBoneIndices() const
	{
		return ClothCollection->GetElements(ClothCollection->GetRenderBoneIndices());
	}

	TConstArrayView<TArray<float>> FCollectionClothConstFacade::GetRenderBoneWeights() const
	{
		return ClothCollection->GetElements(ClothCollection->GetRenderBoneWeights());
	}

	int32 FCollectionClothConstFacade::GetNumRenderFaces() const
	{
		return ClothCollection->GetNumElements(FClothCollection::RenderFacesGroup);
	}

	TConstArrayView<FIntVector3> FCollectionClothConstFacade::GetRenderIndices() const
	{
		return ClothCollection->GetElements(ClothCollection->GetRenderIndices());
	}

	bool FCollectionClothConstFacade::HasWeightMap(const FName& Name) const
	{
		return ClothCollection->HasUserDefinedAttribute<float>(Name, FClothCollection::SimVertices3DGroup);
	}

	TArray<FName> FCollectionClothConstFacade::GetWeightMapNames() const
	{
		return ClothCollection->GetUserDefinedAttributeNames<float>(FClothCollection::SimVertices3DGroup);
	}

	TConstArrayView<float> FCollectionClothConstFacade::GetWeightMap(const FName& Name) const
	{
		return ClothCollection->GetElements(ClothCollection->GetUserDefinedAttribute<float>(Name, FClothCollection::SimVertices3DGroup));
	}

	void FCollectionClothConstFacade::BuildSimulationMesh(TArray<FVector3f>& Positions, TArray<FVector3f>& Normals, TArray<uint32>& Indices, TArray<FVector2f>& PatternsPositions, TArray<uint32>& PatternsIndices,
		TArray<uint32>& PatternToWeldedIndices, TArray<TArray<int32>>* OptionalWeldedToPatternIndices) const
	{
		Positions = GetSimPosition3D();
		Normals = GetSimNormal();
		PatternsPositions = GetSimPosition2D();
		PatternToWeldedIndices = GetSimVertex3DLookup();
		if (OptionalWeldedToPatternIndices)
		{
			*OptionalWeldedToPatternIndices = GetSimVertex2DLookup();
		}

		for (const FIntVector3& Face : GetSimIndices2D())
		{
			PatternsIndices.Add(Face[0]);
			PatternsIndices.Add(Face[1]);
			PatternsIndices.Add(Face[2]);
		}

		// It's possible that welding created degenerate 3D triangles. Remove them when copying over. Keep 2D and 3D triangles in sync
		PatternsIndices.Reset(GetNumSimFaces() * 3);
		Indices.Reset(GetNumSimFaces() * 3);
		for(int32 FaceIdx = 0; FaceIdx < GetNumSimFaces(); ++FaceIdx)
		{
			const FIntVector3 Face3D = GetSimIndices3D()[FaceIdx];

			if (Face3D[0] != Face3D[1] && Face3D[0] != Face3D[2] && Face3D[1] != Face3D[2])
			{
				Indices.Add(Face3D[0]);
				Indices.Add(Face3D[1]);
				Indices.Add(Face3D[2]);

				const FIntVector3 Face2D = GetSimIndices2D()[FaceIdx];
				PatternsIndices.Add(Face2D[0]);
				PatternsIndices.Add(Face2D[1]);
				PatternsIndices.Add(Face2D[2]);
			}
		}
	}

	FCollectionClothFacade::FCollectionClothFacade(const TSharedRef<FManagedArrayCollection>& ManagedArrayCollection)
		: FCollectionClothConstFacade(ManagedArrayCollection)
	{
	}

	FCollectionClothFacade::FCollectionClothFacade(const TSharedRef<FClothCollection>& InClothCollection)
		: FCollectionClothConstFacade(InClothCollection)
	{
	}

	void FCollectionClothFacade::DefineSchema()
	{
		GetClothCollection()->DefineSchema();
		SetDefaults();
	}

	void FCollectionClothFacade::Reset()
	{
		// Hack to reset all Lods data
		GetClothCollection()->SetNumElements(0, FClothCollection::LodsGroup);
		GetClothCollection()->SetNumElements(1, FClothCollection::LodsGroup);
		RemoveAllSimVertices3D();
		SetNumSimPatterns(0);
		SetNumRenderPatterns(0);
		SetNumSeams(0); // Do this after removing SimVertices3D and SimPatterns. Otherwise, Seams will do a bunch of unnecessary work to unseam stuff.
	}

	void FCollectionClothFacade::Initialize(const FCollectionClothConstFacade& Other)
	{
		Reset();

		// LODs Group
		SetPhysicsAssetPathName(Other.GetPhysicsAssetPathName());
		SetSkeletalMeshPathName(Other.GetSkeletalMeshPathName());
		
		Append(Other);
	}

	void FCollectionClothFacade::Append(const FCollectionClothConstFacade& Other)
	{
		// LODs Group 
		// Just keep original data.

		// Very important order of operations to ensure indices don't get messed up:
		// 1) Append 3D Vertices, but don't set 2D Lookups or SeamStitch Lookups because those indices don't exist yet.
		// 2) Append Sim Patterns (includes 2D Vertices, have 3D dependency)
		// 3) Append Seams (have 2D and 3D dependency)
		// 4) Append 2DLookups (2D dep)  and SeamStitchLookups (SeamStitch Dep)

		// Sim Vertices 3D Group
		const int32 StartNumSimVertices3D = GetNumSimVertices3D();
		const int32 OtherNumSimVertices3D = Other.GetNumSimVertices3D();
		GetClothCollection()->SetNumElements(StartNumSimVertices3D + OtherNumSimVertices3D, FClothCollection::SimVertices3DGroup);
		FClothCollection::CopyArrayViewData(GetSimPosition3D().Right(OtherNumSimVertices3D), Other.GetSimPosition3D());
		FClothCollection::CopyArrayViewData(GetSimNormal().Right(OtherNumSimVertices3D), Other.GetSimNormal());
		FClothCollection::CopyArrayViewData(GetSimBoneIndices().Right(OtherNumSimVertices3D), Other.GetSimBoneIndices());
		FClothCollection::CopyArrayViewData(GetSimBoneWeights().Right(OtherNumSimVertices3D), Other.GetSimBoneWeights());
		FClothCollection::CopyArrayViewDataAndApplyOffset(GetTetherKinematicIndex().Right(OtherNumSimVertices3D), Other.GetTetherKinematicIndex(), StartNumSimVertices3D);
		FClothCollection::CopyArrayViewData(GetTetherReferenceLength().Right(OtherNumSimVertices3D), Other.GetTetherReferenceLength());

		// Sim Patterns Group
		const int32 StartNumSimVertices2D = GetNumSimVertices2D();
		const int32 StartNumSimPatterns = GetNumSimPatterns();
		const int32 OtherNumSimPatterns = Other.GetNumSimPatterns();
		SetNumSimPatterns(StartNumSimPatterns + OtherNumSimPatterns);
		for (int32 PatternIndex = 0; PatternIndex < OtherNumSimPatterns; ++PatternIndex)
		{
			GetSimPattern(StartNumSimPatterns + PatternIndex).Initialize(Other.GetSimPattern(PatternIndex), StartNumSimVertices3D);
		}

		// Seams Group
		const int32 StartNumSeams = GetNumSeams();
		const int32 OtherNumSeams = Other.GetNumSeams();
		SetNumSeams(StartNumSeams + OtherNumSeams);
		for (int32 SeamIndex = 0; SeamIndex < OtherNumSeams; ++SeamIndex)
		{
			GetSeam(SeamIndex + StartNumSeams).Initialize(Other.GetSeam(SeamIndex), StartNumSimVertices2D, StartNumSimVertices3D);
		}

		// Sim Vertices 3D Group (lookups)
		FClothCollection::CopyArrayViewDataAndApplyOffset(
			GetClothCollection()->GetElements(GetClothCollection()->GetSimVertex2DLookup()).Right(OtherNumSimVertices3D), 
			Other.GetSimVertex2DLookup(), StartNumSimVertices2D);
		FClothCollection::CopyArrayViewDataAndApplyOffset(
			GetClothCollection()->GetElements(GetClothCollection()->GetSeamStitchLookup()).Right(OtherNumSimVertices3D), 
			Other.GetSeamStitchLookup(), StartNumSeams);

		// Render Patterns Group
		const int32 StartNumRenderPatterns = GetNumRenderPatterns();
		const int32 OtherNumRenderPatterns = Other.GetNumRenderPatterns();
		SetNumRenderPatterns(StartNumRenderPatterns + OtherNumRenderPatterns);
		for (int32 PatternIndex = 0; PatternIndex < OtherNumRenderPatterns; ++PatternIndex)
		{
			GetRenderPattern(PatternIndex + StartNumRenderPatterns).Initialize(Other.GetRenderPattern(PatternIndex));
		}

		// Weight maps
		const TArray<FName> WeightMapNames = Other.GetWeightMapNames();
		for (const FName& WeightMapName : WeightMapNames)
		{
			AddWeightMap(WeightMapName);
			FClothCollection::CopyArrayViewData(GetWeightMap(WeightMapName).Right(OtherNumSimVertices3D), Other.GetWeightMap(WeightMapName));
		}
	}

	void FCollectionClothFacade::SetPhysicsAssetPathName(const FString& PathName)
	{
		if (ClothCollection->GetNumElements(FClothCollection::LodsGroup))
		{
			(*GetClothCollection()->GetPhysicsAssetPathName())[0] = PathName;
		}

	}
	void FCollectionClothFacade::SetSkeletalMeshPathName(const FString& PathName)
	{
		if (ClothCollection->GetNumElements(FClothCollection::LodsGroup))
		{
			(*GetClothCollection()->GetSkeletalMeshPathName())[0] = PathName;
		}
	}

	TArrayView<FVector2f> FCollectionClothFacade::GetSimPosition2D()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetSimPosition2D());
	}

	TArrayView<int32> FCollectionClothFacade::GetSimVertex3DLookupPrivate()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetSimVertex3DLookup());
	}

	TArrayView<FVector3f> FCollectionClothFacade::GetSimPosition3D()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetSimPosition3D());
	}

	TArrayView<FVector3f> FCollectionClothFacade::GetSimNormal()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetSimNormal());
	}

	TArrayView<TArray<int32>> FCollectionClothFacade::GetSimBoneIndices()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetSimBoneIndices());
	}

	TArrayView<TArray<float>> FCollectionClothFacade::GetSimBoneWeights()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetSimBoneWeights());
	}

	TArrayView<TArray<int32>> FCollectionClothFacade::GetTetherKinematicIndex()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetTetherKinematicIndex());
	}

	TArrayView<TArray<float>> FCollectionClothFacade::GetTetherReferenceLength()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetTetherReferenceLength());
	}

	TArrayView<TArray<int32>> FCollectionClothFacade::GetSeamStitchLookupPrivate()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetSeamStitchLookup());
	}

	TArrayView<TArray<int32>> FCollectionClothFacade::GetSimVertex2DLookupPrivate()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetSimVertex2DLookup());
	}

	void FCollectionClothFacade::RemoveSimVertices3D(int32 InNumSimVertices)
	{
		const int32 NumSimVertices = GetNumSimVertices3D();
		check(InNumSimVertices >= NumSimVertices);
		GetClothCollection()->SetNumElements(NumSimVertices - InNumSimVertices, FClothCollection::SimVertices3DGroup);
	}

	void FCollectionClothFacade::RemoveSimVertices3D(const TArray<int32>& SortedDeletionList)
	{
		GetClothCollection()->RemoveElements(FClothCollection::SimVertices3DGroup, SortedDeletionList);
	}

	void FCollectionClothFacade::CompactSimVertex2DLookup()
	{
		TArrayView<TArray<int32>> SimVertex2DLookup = GetSimVertex2DLookupPrivate();
		for (TArray<int32>& VertexLookup : SimVertex2DLookup)
		{
			for (int32 Idx = VertexLookup.Num() - 1; Idx >= 0; --Idx)
			{
				if (VertexLookup[Idx] == INDEX_NONE)
				{
					VertexLookup.RemoveAtSwap(Idx);
				}
			}
		}
	}

	TArrayView<FIntVector3> FCollectionClothFacade::GetSimIndices2D()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetSimIndices2D());
	}

	TArrayView<FIntVector3> FCollectionClothFacade::GetSimIndices3D()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetSimIndices3D());
	}

	void FCollectionClothFacade::SetNumSimPatterns(int32 InNumPatterns)
	{
		const int32 NumPatterns = GetNumSimPatterns();

		for (int32 PatternIndex = InNumPatterns; PatternIndex < NumPatterns; ++PatternIndex)
		{
			GetSimPattern(PatternIndex).Reset();
		}

		GetClothCollection()->SetNumElements(InNumPatterns, FClothCollection::SimPatternsGroup);

		for (int32 PatternIndex = NumPatterns; PatternIndex < InNumPatterns; ++PatternIndex)
		{
			GetSimPattern(PatternIndex).SetDefaults();
		}
	}

	int32 FCollectionClothFacade::AddSimPattern()
	{
		const int32 PatternIndex = GetNumSimPatterns();
		SetNumSimPatterns(PatternIndex + 1);
		return PatternIndex;
	}

	FCollectionClothSimPatternFacade FCollectionClothFacade::GetSimPattern(int32 PatternIndex)
	{
		return FCollectionClothSimPatternFacade(GetClothCollection(), PatternIndex);
	}

	void FCollectionClothFacade::RemoveSimPatterns(const TArray<int32>& SortedDeletionList)
	{
		for (const int32 PatternToRemove : SortedDeletionList)
		{
			GetSimPattern(PatternToRemove).Reset();
		}

		GetClothCollection()->RemoveElements(FClothCollection::SimPatternsGroup, SortedDeletionList);
	}

	void FCollectionClothFacade::SetNumRenderPatterns(int32 InNumPatterns)
	{
		const int32 NumPatterns = GetNumRenderPatterns();

		for (int32 PatternIndex = InNumPatterns; PatternIndex < NumPatterns; ++PatternIndex)
		{
			GetRenderPattern(PatternIndex).Reset();
		}

		GetClothCollection()->SetNumElements(InNumPatterns, FClothCollection::RenderPatternsGroup);

		for (int32 PatternIndex = NumPatterns; PatternIndex < InNumPatterns; ++PatternIndex)
		{
			GetRenderPattern(PatternIndex).SetDefaults();
		}
	}

	int32 FCollectionClothFacade::AddRenderPattern()
	{
		const int32 PatternIndex = GetNumRenderPatterns();
		SetNumRenderPatterns(PatternIndex + 1);
		return PatternIndex;
	}

	FCollectionClothRenderPatternFacade FCollectionClothFacade::GetRenderPattern(int32 PatternIndex)
	{
		return FCollectionClothRenderPatternFacade(GetClothCollection(), PatternIndex);
	}

	void FCollectionClothFacade::RemoveRenderPatterns(const TArray<int32>& SortedDeletionList)
	{
		for (const int32 PatternToRemove : SortedDeletionList)
		{
			GetRenderPattern(PatternToRemove).Reset();
		}

		GetClothCollection()->RemoveElements(FClothCollection::RenderPatternsGroup, SortedDeletionList);
	}

	TArrayView<FString> FCollectionClothFacade::GetRenderMaterialPathName()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetRenderMaterialPathName());
	}

	void FCollectionClothFacade::SetNumSeams(int32 InNumSeams)
	{
		const int32 NumSeams = GetNumSeams();

		for (int32 SeamIndex = InNumSeams; SeamIndex < NumSeams; ++SeamIndex)
		{
			GetSeam(SeamIndex).Reset();
		}

		GetClothCollection()->SetNumElements(InNumSeams, FClothCollection::SeamsGroup);

		for (int32 SeamIndex = NumSeams; SeamIndex < InNumSeams; ++SeamIndex)
		{
			GetSeam(SeamIndex).SetDefaults();
		}
	}

	int32 FCollectionClothFacade::AddSeam()
	{
		const int32 SeamIndex = GetNumSeams();
		SetNumSeams(SeamIndex + 1);
		return SeamIndex;
	}

	FCollectionClothSeamFacade FCollectionClothFacade::GetSeam(int32 SeamIndex)
	{
		return FCollectionClothSeamFacade(GetClothCollection(), SeamIndex);
	}

	void FCollectionClothFacade::RemoveSeams(const TArray<int32>& SortedDeletionList)
	{
		for (const int32 SeamToRemove : SortedDeletionList)
		{
			GetSeam(SeamToRemove).Reset();
		}
		GetClothCollection()->RemoveElements(FClothCollection::SeamsGroup, SortedDeletionList);
	}

	//~ Render Vertices Group
	TArrayView<FVector3f> FCollectionClothFacade::GetRenderPosition()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetRenderPosition());
	}

	TArrayView<FVector3f> FCollectionClothFacade::GetRenderNormal()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetRenderNormal());
	}

	TArrayView<FVector3f> FCollectionClothFacade::GetRenderTangentU()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetRenderTangentU());
	}

	TArrayView<FVector3f> FCollectionClothFacade::GetRenderTangentV()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetRenderTangentV());
	}

	TArrayView<TArray<FVector2f>> FCollectionClothFacade::GetRenderUVs()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetRenderUVs());
	}

	TArrayView<FLinearColor> FCollectionClothFacade::GetRenderColor()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetRenderColor());
	}

	TArrayView<TArray<int32>> FCollectionClothFacade::GetRenderBoneIndices()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetRenderBoneIndices());
	}

	TArrayView<TArray<float>> FCollectionClothFacade::GetRenderBoneWeights()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetRenderBoneWeights());
	}

	TArrayView<FIntVector3> FCollectionClothFacade::GetRenderIndices()
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetRenderIndices());
	}

	void FCollectionClothFacade::AddWeightMap(const FName& Name)
	{
		check(IsValid());
		GetClothCollection()->AddUserDefinedAttribute<float>(Name, FClothCollection::SimVertices3DGroup);
	}

	void FCollectionClothFacade::RemoveWeightMap(const FName& Name)
	{
		check(IsValid());
		GetClothCollection()->RemoveUserDefinedAttribute(Name, FClothCollection::SimVertices3DGroup);
	}

	TArrayView<float> FCollectionClothFacade::GetWeightMap(const FName& Name)
	{
		return GetClothCollection()->GetElements(GetClothCollection()->GetUserDefinedAttribute<float>(Name, FClothCollection::SimVertices3DGroup));
	}

	void FCollectionClothFacade::SetDefaults()
	{
		GetClothCollection()->SetNumElements(1, FClothCollection::LodsGroup);
	}

} // End namespace UE::Chaos::ClothAsset
