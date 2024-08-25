// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection.cpp: FGeometryCollection methods.
=============================================================================*/

#include "GeometryCollection/Facades/CollectionBoundsFacade.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/Facades/CollectionHierarchyFacade.h"
#include "GeometryCollection/Facades/CollectionTransformFacade.h"

namespace GeometryCollection::Facades
{

	FBoundsFacade::FBoundsFacade(FManagedArrayCollection& InCollection)
		: ConstCollection(InCollection)
		, Collection(&InCollection)
		, BoundingBoxAttribute(InCollection,"BoundingBox", FGeometryCollection::GeometryGroup)
		, VertexAttribute(InCollection,"Vertex", FGeometryCollection::VerticesGroup, FGeometryCollection::VerticesGroup)
		, BoneMapAttribute(InCollection,"BoneMap", FGeometryCollection::VerticesGroup)
		, TransformToGeometryIndexAttribute(InCollection,"TransformToGeometryIndex", FTransformCollection::TransformGroup)
		, VertexStartAttribute(InCollection, "VertexStart", FGeometryCollection::GeometryGroup)
		, VertexCountAttribute(InCollection, "VertexCount", FGeometryCollection::GeometryGroup)
	{}

	FBoundsFacade::FBoundsFacade(const FManagedArrayCollection& InCollection)
		: ConstCollection(InCollection)
		, Collection(nullptr)
		, BoundingBoxAttribute(InCollection, "BoundingBox", FGeometryCollection::GeometryGroup)
		, VertexAttribute(InCollection, "Vertex", FGeometryCollection::VerticesGroup, FGeometryCollection::VerticesGroup)
		, BoneMapAttribute(InCollection, "BoneMap", FGeometryCollection::VerticesGroup)
		, TransformToGeometryIndexAttribute(InCollection, "TransformToGeometryIndex", FTransformCollection::TransformGroup)
		, VertexStartAttribute(InCollection, "VertexStart", FGeometryCollection::GeometryGroup)
		, VertexCountAttribute(InCollection, "VertexCount", FGeometryCollection::GeometryGroup)
	{}

	//
	//  Initialization
	//

	void FBoundsFacade::DefineSchema()
	{
		check(!IsConst());
		BoundingBoxAttribute.Add();
	}

	bool FBoundsFacade::IsValid() const
	{
		return BoundingBoxAttribute.IsValid();
	}


	void FBoundsFacade::UpdateBoundingBox()
	{
		if (BoundingBoxAttribute.IsValid())
		{
			bool bTranformBased = VertexAttribute.IsValid() && TransformToGeometryIndexAttribute.Num() && BoneMapAttribute.IsValid();

			bool bVertexBased = VertexAttribute.IsValid() && VertexStartAttribute.Num() && VertexCountAttribute.IsValid();

			if (bTranformBased)
			{
				UpdateTransformBasedBoundingBox();
			}
			else if (bVertexBased)
			{
				UpdateVertexBasedBoundingBox();
			}
			else
			{
				TManagedArray<FBox>& BoundingBox = BoundingBoxAttribute.Modify();
				for (int32 bdx = 0; bdx < BoundingBox.Num(); bdx++)
				{
					BoundingBox[bdx].Init();
				}
			}
		}
	}

	void FBoundsFacade::UpdateTransformBasedBoundingBox()
	{
		TManagedArray<FBox>& BoundingBox = BoundingBoxAttribute.Modify();
		const TManagedArray<FVector3f>& Vertex = VertexAttribute.Get();
		const TManagedArray<int32>& BoneMap = BoneMapAttribute.Get();
		const TManagedArray<int32>& TransformToGeometryIndex = TransformToGeometryIndexAttribute.Get();

		for (int32 bdx = 0; bdx < BoundingBox.Num(); bdx++)
		{
			BoundingBox[bdx].Init();
		}

		// Use the mapping stored from the vertices to the transforms to generate a bounding box
		// relative to transform origin. 

		if (BoundingBox.Num())
		{
			// Compute BoundingBox
			for (int32 Idx = 0; Idx < Vertex.Num(); ++Idx)
			{
				int32 TransformIndexValue = BoneMap[Idx];
				BoundingBox[TransformToGeometryIndex[TransformIndexValue]] += FVector(Vertex[Idx]);
			}
		}
	}

	void FBoundsFacade::UpdateVertexBasedBoundingBox()
	{
		TManagedArray<FBox>& BoundingBox = BoundingBoxAttribute.Modify();
		const TManagedArray<FVector3f>& Vertex = VertexAttribute.Get();
		const TManagedArray<int32>& VertexStart = VertexStartAttribute.Get();
		const TManagedArray<int32>& VertexCount = VertexCountAttribute.Get();

		for (int32 bdx = 0; bdx < BoundingBox.Num(); bdx++)
		{
			BoundingBox[bdx].Init();
		}

		// Use the mapping stored from the geometry to the vertices to generate a bounding box.
		// This configuration might not have an assiocated transform.

		for (int32 Gdx = 0; Gdx < BoundingBox.Num(); Gdx++)
		{
			// Compute BoundingBox
			int32 VertexEnd = VertexStart[Gdx] + VertexCount[Gdx];
			for (int32 Vdx = VertexStart[Gdx]; Vdx < VertexEnd; ++Vdx)
			{
				BoundingBox[Gdx] += FVector(Vertex[Vdx]);
			}
		}
	}

	TArray<FVector> FBoundsFacade::GetCentroids() const
	{
		TArray<FVector> Centroids;

		if (IsValid())
		{
			const TManagedArray<FBox>& BoundingBoxes = BoundingBoxAttribute.Get();

			for (int32 Idx = 0; Idx < BoundingBoxes.Num(); ++Idx)
			{
				Centroids.Add(BoundingBoxes[Idx].GetCenter());
			}
		}
		
		return Centroids;
	}

	FBox FBoundsFacade::GetBoundingBoxInCollectionSpace() const
	{
		FBox BoundingBox;
		BoundingBox.Init();

		if (IsValid())
		{
			GeometryCollection::Facades::FCollectionTransformFacade TransformFacade(ConstCollection);
			if (TransformFacade.IsValid())
			{
				TArray<FTransform> CollectionSpaceTransforms = TransformFacade.ComputeCollectionSpaceTransforms();

				for (int32 TransformIndex = 0; TransformIndex < CollectionSpaceTransforms.Num(); ++TransformIndex)
				{
					const int32 GeoIndex = TransformToGeometryIndexAttribute[TransformIndex];
					if (BoundingBoxAttribute.IsValidIndex(GeoIndex))
					{
						const FBox& GeoBoundingBox = BoundingBoxAttribute[GeoIndex];
						const FTransform CollectionSpaceTransform = CollectionSpaceTransforms[TransformIndex];
						const FBox BoundingBoxInCollectionSpace = GeoBoundingBox.TransformBy(CollectionSpaceTransform);
						BoundingBox += BoundingBoxInCollectionSpace;
					}
				}
			}
			else
			{
				const TManagedArray<FBox>& BoundingBoxes = BoundingBoxAttribute.Get();
				for (int32 Idx = 0; Idx < BoundingBoxes.Num(); ++Idx)
				{
					BoundingBox += BoundingBoxes[Idx];
				}
			}
		}

		return BoundingBox;
	}

	TArray<FVector> FBoundsFacade::GetBoundingBoxVertexPositions(const FBox& InBox)
	{
		FVector Min = InBox.Min;
		FVector Max = InBox.Max;
		FVector Extent(Max.X - Min.X, Max.Y - Min.Y, Max.Z - Min.Z);

		TArray<FVector> Vertices;
		Vertices.Add(Min);
		Vertices.Add({ Min.X + Extent.X, Min.Y, Min.Z });
		Vertices.Add({ Min.X + Extent.X, Min.Y + Extent.Y, Min.Z });
		Vertices.Add({ Min.X, Min.Y + Extent.Y, Min.Z });
		Vertices.Add({ Min.X, Min.Y, Min.Z + Extent.Z });
		Vertices.Add({ Min.X + Extent.X, Min.Y, Min.Z + Extent.Z });
		Vertices.Add(Max);
		Vertices.Add({ Min.X, Min.Y + Extent.Y, Min.Z + Extent.Z });

		return Vertices;
	}

}; // GeometryCollection::Facades


