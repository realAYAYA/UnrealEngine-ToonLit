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
		, ParentAttribute(InCollection, "Parent", FTransformCollection::TransformGroup)
	{}

	FBoundsFacade::FBoundsFacade(const FManagedArrayCollection& InCollection)
		: ConstCollection(InCollection)
		, Collection(nullptr)
		, BoundingBoxAttribute(InCollection, "BoundingBox", FGeometryCollection::GeometryGroup)
		, VertexAttribute(InCollection, "Vertex", FGeometryCollection::VerticesGroup, FGeometryCollection::VerticesGroup)
		, BoneMapAttribute(InCollection, "BoneMap", FGeometryCollection::VerticesGroup)
		, TransformToGeometryIndexAttribute(InCollection, "TransformToGeometryIndex", FTransformCollection::TransformGroup)
		, ParentAttribute(InCollection, "Parent", FTransformCollection::TransformGroup)
	{}

	//
	//  Initialization
	//

	void FBoundsFacade::DefineSchema()
	{
		check(!IsConst());
		BoundingBoxAttribute.Add();
		VertexAttribute.Add();
		BoneMapAttribute.Add();
		TransformToGeometryIndexAttribute.Add();
		ParentAttribute.Add();
	}

	bool FBoundsFacade::IsValid() const
	{
		return BoundingBoxAttribute.IsValid() && VertexAttribute.IsValid() 
			&& BoneMapAttribute.IsValid() && TransformToGeometryIndexAttribute.IsValid()
			&& ParentAttribute.IsValid();
	}


	void FBoundsFacade::UpdateBoundingBox(bool bSkipCheck)
	{
		check(!IsConst());

		if (!bSkipCheck || !IsValid())
		{
			return;
		}

		TManagedArray<FBox>& BoundingBox = BoundingBoxAttribute.Modify();
		const TManagedArray<FVector3f>& Vertex = VertexAttribute.Get();
		const TManagedArray<int32>& BoneMap = BoneMapAttribute.Get();
		const TManagedArray<int32>& TransformToGeometryIndex = TransformToGeometryIndexAttribute.Get();

		if (BoundingBox.Num())
		{
			// Initialize BoundingBox
			for (int32 Idx = 0; Idx < BoundingBox.Num(); ++Idx)
			{
				BoundingBox[Idx].Init();
			}

			// Compute BoundingBox
			for (int32 Idx = 0; Idx < Vertex.Num(); ++Idx)
			{
				int32 TransformIndexValue = BoneMap[Idx];
				BoundingBox[TransformToGeometryIndex[TransformIndexValue]] += FVector(Vertex[Idx]);
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

	FBox FBoundsFacade::GetBoundingBox() const
	{
		FBox BoundingBox;

		if (IsValid())
		{
			const TArray<int32>& RootIndices = Chaos::Facades::FCollectionHierarchyFacade::GetRootIndices(ParentAttribute);

			const TManagedArray<FBox>& BoundingBoxArr = BoundingBoxAttribute.Get();

			BoundingBox.Init();

			for (int32 Idx : RootIndices)
			{
				BoundingBox += BoundingBoxArr[Idx];
			}
		}

		return BoundingBox;
	}

	FBox FBoundsFacade::GetBoundingBoxInCollectionSpace() const
	{
		FBox BoundingBox;

		if (IsValid())
		{
			const TArray<int32>& RootIndices = Chaos::Facades::FCollectionHierarchyFacade::GetRootIndices(ParentAttribute);

			const TManagedArray<FBox>& BoundingBoxArr = BoundingBoxAttribute.Get();

			GeometryCollection::Facades::FCollectionTransformFacade TransformFacade(ConstCollection);

			BoundingBox.Init();

			for (int32 Idx : RootIndices)
			{
				const FTransform CollectionSpaceTransform = TransformFacade.ComputeCollectionSpaceTransform(Idx);
				const FBox BoundingBoxInCollectionSpace = BoundingBoxArr[Idx].TransformBy(CollectionSpaceTransform);

				BoundingBox += BoundingBoxInCollectionSpace;
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


