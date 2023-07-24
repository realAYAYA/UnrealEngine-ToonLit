// Copyright Epic Games, Inc. All Rights Reserved. 

#if WITH_EDITOR

#include "ChaosClothAsset/ClothPatternToDynamicMesh.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothCollection.h"
#include "ChaosClothAsset/ClothAdapter.h"
#include "ChaosClothAsset/ClothLodAdapter.h"
#include "ChaosClothAsset/ClothPatternAdapter.h"
#include "ToDynamicMesh.h"
#include "DynamicMesh/MeshNormals.h"

//
// Wrapper for accessing a Cloth Pattern. Implements the interface expected by TToDynamicMesh<>.
//
class FClothSimPatternWrapper
{
public:

	typedef int32 TriIDType;
	typedef int32 VertIDType;
	typedef int32 WedgeIDType;

	typedef int32 UVIDType;
	typedef int32 NormalIDType;
	typedef int32 ColorIDType;

	FClothSimPatternWrapper(const UE::Chaos::ClothAsset::FClothPatternConstAdapter& PatternIn, bool bGet2DPatternVertices) :
		bGet2DPatternVertices(bGet2DPatternVertices),
		Pattern(PatternIn),
		Lod(PatternIn.GetClothCollection(), PatternIn.GetLodIndex())
	{
		using UE::Chaos::ClothAsset::FClothCollection;

		const int32 PatternIndex = Pattern.GetPatternIndex();

		VertIDs.SetNum(Pattern.GetNumSimVertices());
		for (int32 VtxIndex = 0; VtxIndex < Pattern.GetNumSimVertices(); ++VtxIndex)
		{
			const int32 PatternOffset = Lod.GetSimVerticesStart()[PatternIndex];
			VertIDs[VtxIndex] = PatternOffset + VtxIndex;
		}

		TriIDs.SetNum(Pattern.GetNumSimFaces());
		for (int32 TriIndex = 0; TriIndex < Pattern.GetNumSimFaces(); ++TriIndex)
		{
			TriIDs[TriIndex] = TriIndex;
		}


		//
		// Weight map layers precomputation
		//
		const FClothCollection* ClothCollection = Pattern.GetClothCollection().Get();
		const TArray<FName> SimVerticesAttributeNames = ClothCollection->AttributeNames(FClothCollection::SimVerticesGroup);

		// Of the SimVerticesGroup attributes, which ones refer to scalar weight maps?
		for (const FName& AttributeName : SimVerticesAttributeNames)
		{
			if (ClothCollection->FindAttributeTyped<float>(AttributeName, FClothCollection::SimVerticesGroup))
			{
				WeightMapNames.Add(AttributeName);
			}
		}
	}

	int32 NumTris() const
	{
		return TriIDs.Num();
	}

	int32 NumVerts() const
	{
		return VertIDs.Num();
	}

	int32 NumUVLayers() const
	{
		return 0;	// No UVs for Sim mesh
	}

	int32 NumWeightMapLayers() const
	{
		return WeightMapNames.Num();
	}

	FName GetWeightMapName(int32 LayerIndex) const
	{
		return WeightMapNames[LayerIndex];
	}

	float GetVertexWeight(int32 LayerIndex, int32 VertexIndex) const
	{
		using UE::Chaos::ClothAsset::FClothCollection;

		const FName& AttributeName = WeightMapNames[LayerIndex];
		const FClothCollection* ClothCollection = Pattern.GetClothCollection().Get();
		ensure(ClothCollection->FindAttributeTyped<float>(AttributeName, FClothCollection::SimVerticesGroup));
		
		const TManagedArray<float>& ManagedArray = ClothCollection->GetAttribute<float>(AttributeName, FClothCollection::SimVerticesGroup);
		return ManagedArray[VertexIndex];
	}

	// --"Vertex Buffer" info
	const TArray<VertIDType>& GetVertIDs() const
	{
		return VertIDs;
	}

	FVector3d GetPosition(VertIDType VtxID) const
	{
		if (bGet2DPatternVertices)
		{
			TConstArrayView<FVector2f> SimPositions = Lod.GetPatternsSimPosition();
			const FVector2f& Pos = SimPositions[VtxID];
			return FVector3d(Pos[0], Pos[1], 0.0);
		} 
		else
		{
			TConstArrayView<FVector3f> RestPositions = Lod.GetPatternsSimRestPosition();
			const FVector3f& Pos = RestPositions[VtxID];
			return FVector3d(Pos[0], Pos[1], Pos[2]);
		}
	}

	// --"Index Buffer" info
	const TArray<TriIDType>& GetTriIDs() const
	{
		return TriIDs;
	}

	bool GetTri(TriIDType TriID, VertIDType& VID0, VertIDType& VID1, VertIDType& VID2) const
	{
		const FIntVector Face = Pattern.GetSimIndices()[TriID];
		VID0 = Face[0];
		VID1 = Face[1];
		VID2 = Face[2];

		return true;
	}
	
	bool HasNormals() const
	{
		return true;
	}

	bool HasTangents() const
	{
		return false;
	}

	bool HasBiTangents() const
	{
		return false;
	}

	bool HasColors() const
	{
		return false;
	}

	// -- Access to per-wedge attributes -- //
	void GetWedgeIDs(const TriIDType& TriID, WedgeIDType& WID0, WedgeIDType& WID1, WedgeIDType& WID2) const
	{
		// TODO: Review this
		int32 Offset = 3 * TriID;
		WID0 = Offset;
		WID1 = Offset + 1;
		WID2 = Offset + 2;
	}

	FVector2f GetWedgeUV(int32 UVLayerIndex, WedgeIDType WID) const
	{
		ensure(false);
		return FVector2f(0.0f, 0.0f); 
	}

	FVector3f GetWedgeNormal(WedgeIDType WID) const
	{
		ensure(false);
		return FVector3f(0.0f, 0.0f, 0.0f);
	}

	FVector3f GetWedgeTangent(WedgeIDType WID) const
	{
		ensure(false);
		return FVector3f(0.0f, 0.0f, 0.0f);
	}

	FVector3f GetWedgeBiTangent(WedgeIDType WID) const
	{
		ensure(false);
		return FVector3f(0.0f, 0.0f, 0.0f);
	}
	
	FVector4f GetWedgeColor(WedgeIDType WID) const
	{
		ensure(false);
		return FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
	}

	int32 GetMaterialIndex(TriIDType TriID) const
	{
		return 0;
	}

	const TArray<int32>& GetNormalIDs() const { return VertIDs; }
	FVector3f GetNormal(NormalIDType ID) const { return Lod.GetPatternsSimRestNormal()[ID]; }
	bool GetNormalTri(const TriIDType& TriID, NormalIDType& ID0, NormalIDType& ID1, NormalIDType& ID2) const { return GetTri(TriID, ID0, ID1, ID2); }
	
	const TArray<int32>& GetUVIDs(int32 LayerID) const { return EmptyArray; }
	FVector2f GetUV(int32 LayerID, UVIDType UVID) const { check(0); return FVector2f(); }
	bool GetUVTri(int32 LayerID, const TriIDType&, UVIDType& ID0, UVIDType& ID1, UVIDType& ID2) const { ID0 = ID1 = ID2 = UVIDType(-1); return false;}
	
	const TArray<int32>& GetTangentIDs() const { return EmptyArray; }
	FVector3f GetTangent(NormalIDType ID) const { check(0); return FVector3f(); }
	bool GetTangentTri(const TriIDType&, NormalIDType& ID0, NormalIDType& ID1, NormalIDType& ID2) const { ID0 = ID1 = ID2 = NormalIDType(-1); return false; }

	const TArray<int32>& GetBiTangentIDs() const { return EmptyArray; }
	FVector3f GetBiTangent(NormalIDType ID) const {check(0); return FVector3f(); }
	bool GetBiTangentTri(const TriIDType&, NormalIDType& ID0, NormalIDType& ID1, NormalIDType& ID2) const { ID0 = ID1 = ID2 = NormalIDType(-1); return false; }

	const TArray<int32>& GetColorIDs() const { return EmptyArray; }
	FVector4f GetColor(ColorIDType ID) const { check(0); return FVector4f(); }
	bool GetColorTri(const TriIDType&, ColorIDType& ID0, ColorIDType& ID1, ColorIDType& ID2) const { ID0 = ID1 = ID2 = ColorIDType(-1); return false; }

private:

	bool bGet2DPatternVertices = false;

	TArray<TriIDType> TriIDs;		// indices into Pattern.GetSimIndices() 
	TArray<VertIDType> VertIDs;		// indices into Lod.GetPatternsSimPosition(), etc.
	
	TArray<FName> WeightMapNames;

	const UE::Chaos::ClothAsset::FClothPatternConstAdapter Pattern;
	const UE::Chaos::ClothAsset::FClothLodConstAdapter Lod;

	TArray<int32> EmptyArray;
};


void FClothPatternToDynamicMesh::Convert(const UChaosClothAsset* ClothAssetMeshIn, int32 LODIndex, int32 PatternIndex, bool bGet2DPattern, UE::Geometry::FDynamicMesh3& MeshOut)
{
	const TSharedPtr<const UE::Chaos::ClothAsset::FClothCollection> ClothCollection = ClothAssetMeshIn->GetClothCollection();
	check(ClothCollection.IsValid());
	const UE::Chaos::ClothAsset::FClothConstAdapter ClothAdapter(ClothCollection);

	const UE::Chaos::ClothAsset::FClothLodConstAdapter ClothLodAdapter = ClothAdapter.GetLod(LODIndex);
	const UE::Chaos::ClothAsset::FClothPatternConstAdapter ClothPatternAdapter = ClothLodAdapter.GetPattern(PatternIndex);

	// Actual conversion
	UE::Geometry::TToDynamicMesh<FClothSimPatternWrapper> PatternToDynamicMesh; 
	FClothSimPatternWrapper PatternWrapper(ClothPatternAdapter, bGet2DPattern);

	const bool bDisableAttributes = false;
	auto TriangleToGroupFunction = [](FClothSimPatternWrapper::TriIDType) { return 0; };

	if (bDisableAttributes)
	{
		MeshOut.DiscardAttributes();
		PatternToDynamicMesh.ConvertWOAttributes(MeshOut, PatternWrapper, TriangleToGroupFunction);
	}
	else
	{
		MeshOut.EnableAttributes();

		auto TriangleToMaterialFunction = [](FClothSimPatternWrapper::TriIDType) { return 0; };
		constexpr bool bCopyTangents = false;
		PatternToDynamicMesh.Convert(MeshOut, PatternWrapper, TriangleToGroupFunction, TriangleToMaterialFunction, bCopyTangents);
	}
}


#else

void  FClothAssetToDynamicMesh::Convert(const UChaosClothAsset* ClothAssetMeshIn, FDynamicMesh3& MeshOut, int32 LODIndex, int32 PatternIndex)
{
	// Conversion only supported with editor.
	check(0);
}

#endif  // end with editor
