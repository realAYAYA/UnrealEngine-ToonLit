// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection.cpp: FGeometryCollection methods.
=============================================================================*/

#include "GeometryCollection/Facades/CollectionMeshFacade.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/TransformCollection.h"

namespace GeometryCollection::Facades
{
	FCollectionMeshFacade::FCollectionMeshFacade(FManagedArrayCollection& InCollection)
		: FCollectionUVFacade(InCollection)
		, TransformToGeometryIndexAttribute(InCollection, "TransformToGeometryIndex", FGeometryCollection::TransformGroup)
		, TransformIndexAttribute(InCollection, "TransformIndex", FGeometryCollection::GeometryGroup)
		, VertexAttribute(InCollection, "Vertex", FGeometryCollection::VerticesGroup)
		, TangentUAttribute(InCollection, "TangentU", FGeometryCollection::VerticesGroup)
		, TangentVAttribute(InCollection, "TangentV", FGeometryCollection::VerticesGroup)
		, NormalAttribute(InCollection, "Normal", FGeometryCollection::VerticesGroup)
		, ColorAttribute(InCollection, "Color", FGeometryCollection::VerticesGroup)
		, BoneMapAttribute(InCollection, "BoneMap", FGeometryCollection::VerticesGroup)
		, VertexStartAttribute(InCollection, "VertexStart", FGeometryCollection::GeometryGroup)
		, VertexCountAttribute(InCollection, "VertexCount", FGeometryCollection::GeometryGroup)
		, IndicesAttribute(InCollection, "Indices", FGeometryCollection::FacesGroup)
		, VisibleAttribute(InCollection, "Visible", FGeometryCollection::FacesGroup)
		, MaterialIndexAttribute(InCollection, "MaterialIndex", FGeometryCollection::FacesGroup)
		, MaterialIDAttribute(InCollection, "MaterialID", FGeometryCollection::FacesGroup)
		, InternalAttribute(InCollection, "Internal", FGeometryCollection::FacesGroup)
		, FaceStartAttribute(InCollection, "FaceStart", FGeometryCollection::GeometryGroup)
		, FaceCountAttribute(InCollection, "FaceCount", FGeometryCollection::GeometryGroup)
	{
	}

	FCollectionMeshFacade::FCollectionMeshFacade(const FManagedArrayCollection& InCollection)
		: FCollectionUVFacade(InCollection)
		, TransformToGeometryIndexAttribute(InCollection, "TransformToGeometryIndex", FGeometryCollection::TransformGroup)
		, TransformIndexAttribute(InCollection, "TransformIndex", FGeometryCollection::GeometryGroup)
		, VertexAttribute(InCollection, "Vertex", FGeometryCollection::VerticesGroup)
		, TangentUAttribute(InCollection, "TangentU", FGeometryCollection::VerticesGroup)
		, TangentVAttribute(InCollection, "TangentV", FGeometryCollection::VerticesGroup)
		, NormalAttribute(InCollection, "Normal", FGeometryCollection::VerticesGroup)
		, ColorAttribute(InCollection, "Color", FGeometryCollection::VerticesGroup)
		, BoneMapAttribute(InCollection, "BoneMap", FGeometryCollection::VerticesGroup)
		, VertexStartAttribute(InCollection, "VertexStart", FGeometryCollection::GeometryGroup)
		, VertexCountAttribute(InCollection, "VertexCount", FGeometryCollection::GeometryGroup)
		, IndicesAttribute(InCollection, "Indices", FGeometryCollection::FacesGroup)
		, VisibleAttribute(InCollection, "Visible", FGeometryCollection::FacesGroup)
		, MaterialIndexAttribute(InCollection, "MaterialIndex", FGeometryCollection::FacesGroup)
		, MaterialIDAttribute(InCollection, "MaterialID", FGeometryCollection::FacesGroup)
		, InternalAttribute(InCollection, "Internal", FGeometryCollection::FacesGroup)
		, FaceStartAttribute(InCollection, "FaceStart", FGeometryCollection::GeometryGroup)
		, FaceCountAttribute(InCollection, "FaceCount", FGeometryCollection::GeometryGroup)
	{
	}

	bool FCollectionMeshFacade::IsValid() const
	{
		return FCollectionUVFacade::IsValid()
			&& TransformToGeometryIndexAttribute.IsValid()
			&& TransformIndexAttribute.IsValid()
			&& VertexAttribute.IsValid()
			&& TangentUAttribute.IsValid()
			&& TangentVAttribute.IsValid()
			&& NormalAttribute.IsValid()
			&& ColorAttribute.IsValid()
			&& BoneMapAttribute.IsValid()
			&& VertexStartAttribute.IsValid()
			&& VertexCountAttribute.IsValid()
			&& IndicesAttribute.IsValid()
			&& VisibleAttribute.IsValid()
			&& MaterialIndexAttribute.IsValid()
			&& MaterialIDAttribute.IsValid()
			&& InternalAttribute.IsValid()
			&& FaceStartAttribute.IsValid()
			&& FaceCountAttribute.IsValid()
			;
	}

	void FCollectionMeshFacade::DefineSchema()
	{
		FCollectionUVFacade::DefineSchema();
		TransformToGeometryIndexAttribute.Add();
		TransformIndexAttribute.Add();
		VertexAttribute.Add();
		TangentUAttribute.Add();
		TangentVAttribute.Add();
		NormalAttribute.Add();
		ColorAttribute.Add();
		BoneMapAttribute.Add();
		VertexStartAttribute.Add();
		VertexCountAttribute.Add();
		IndicesAttribute.Add();
		VisibleAttribute.Add();
		MaterialIndexAttribute.Add();
		MaterialIDAttribute.Add();
		InternalAttribute.Add();
		FaceStartAttribute.Add();
		FaceCountAttribute.Add();
	}

	const TArray<int32> FCollectionMeshFacade::GetVertexIndices(int32 BoneIdx) const
	{
		const TManagedArray<int32>& TransformToGeometryIndicies = TransformToGeometryIndexAttribute.Get();
		const TManagedArray<int32>& VertexStarts = VertexStartAttribute.Get();
		const TManagedArray<int32>& VertexCounts = VertexCountAttribute.Get();

		TArray<int32> VertexIndices;

		const int32 VertexIndexStart = VertexStarts[TransformToGeometryIndicies[BoneIdx]];
		for (int32 Offset = 0; Offset < VertexCounts[TransformToGeometryIndicies[BoneIdx]]; ++Offset)
		{
			VertexIndices.Add(VertexIndexStart + Offset);
		}

		return VertexIndices;
	}

	const TArrayView<const FVector3f> FCollectionMeshFacade::GetVertexPositions(int32 BoneIdx) const
	{
		const TManagedArray<int32>& TransformToGeometryIndicies = TransformToGeometryIndexAttribute.Get();
		const TManagedArray<int32>& VertexStarts = VertexStartAttribute.Get();
		const TManagedArray<int32>& VertexCounts = VertexCountAttribute.Get();
		const TManagedArray<FVector3f>& Vertices = VertexAttribute.Get();

		return TArrayView<const FVector3f>(Vertices.GetData() + VertexStarts[TransformToGeometryIndicies[BoneIdx]], VertexCounts[TransformToGeometryIndicies[BoneIdx]] + 1);
	}

	const TArray<int32> FCollectionMeshFacade::GetFaceIndices(int32 BoneIdx) const
	{
		const TManagedArray<int32>& TransformToGeometryIndicies = TransformToGeometryIndexAttribute.Get();
		const TManagedArray<int32>& FaceStarts = FaceStartAttribute.Get();
		const TManagedArray<int32>& FaceCounts = FaceCountAttribute.Get();

		TArray<int32> FaceIndicies;

		const int32 FaceIndexStart = FaceStarts[TransformToGeometryIndicies[BoneIdx]];
		for (int32 Offset = 0; Offset < FaceCounts[TransformToGeometryIndicies[BoneIdx]]; ++Offset)
		{
			FaceIndicies.Add(FaceIndexStart + Offset);
		}

		return FaceIndicies;
	}

	void FCollectionMeshFacade::BakeTransform(int32 TransformIdx, const FTransform& InTransform)
	{
		const TManagedArray<int32>& TransformToGeometryIndices = TransformToGeometryIndexAttribute.Get();
		TManagedArray<FVector3f>& Vertices = VertexAttribute.Modify();
		const TManagedArray<int32>& VertexStarts = VertexStartAttribute.Get();
		const TManagedArray<int32>& VertexCounts = VertexCountAttribute.Get();
		TManagedArray<FVector3f>& Normals = NormalAttribute.Modify();
		TManagedArray<FVector3f>& TangentUs = TangentUAttribute.Modify();
		TManagedArray<FVector3f>& TangentVs = TangentVAttribute.Modify();

		const int32 VertexIndexStart = VertexStarts[TransformToGeometryIndices[TransformIdx]];

		for (int32 Offset = 0; Offset < VertexCounts[TransformToGeometryIndices[TransformIdx]]; ++Offset)
		{
			const int32 VertexIdx = VertexIndexStart + Offset;

			Vertices[VertexIdx] = (FVector3f)InTransform.TransformPosition((FVector)Vertices[VertexIdx]);
			Normals[VertexIdx] = (FVector3f)InTransform.TransformVector((FVector)Normals[VertexIdx]);
			TangentUs[VertexIdx] = (FVector3f)InTransform.TransformVector((FVector)TangentUs[VertexIdx]);
			TangentVs[VertexIdx] = (FVector3f)InTransform.TransformVector((FVector)TangentVs[VertexIdx]);
		}
	}

};


