// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection.cpp: FGeometryCollection methods.
=============================================================================*/

#include "GeometryCollection/Facades/CollectionVertexBoneWeightsFacade.h"
#include "GeometryCollection/Facades/CollectionKinematicBindingFacade.h"
#include "GeometryCollection/GeometryCollection.h"

namespace GeometryCollection::Facades
{

	// Attributes
	const FName FVertexBoneWeightsFacade::BoneWeightAttributeName = "BoneWeights";
	const FName FVertexBoneWeightsFacade::BoneIndexAttributeName = "BoneWeightsIndex";

	FVertexBoneWeightsFacade::FVertexBoneWeightsFacade(FManagedArrayCollection& InCollection)
		: ConstCollection(InCollection)
		, Collection(&InCollection)
		, BoneIndexAttribute(InCollection, BoneIndexAttributeName, FGeometryCollection::VerticesGroup, FTransformCollection::TransformGroup)
		, BoneWeightAttribute(InCollection, BoneWeightAttributeName, FGeometryCollection::VerticesGroup, FTransformCollection::TransformGroup)
		, ParentAttribute(InCollection, FTransformCollection::ParentAttribute, FTransformCollection::TransformGroup)
		, VerticesAttribute(InCollection, "Vertex", FGeometryCollection::VerticesGroup)
	{
		DefineSchema();
	}

	FVertexBoneWeightsFacade::FVertexBoneWeightsFacade(const FManagedArrayCollection& InCollection)
		: ConstCollection(InCollection)
		, Collection(nullptr)
		, BoneIndexAttribute(InCollection, BoneIndexAttributeName, FGeometryCollection::VerticesGroup, FTransformCollection::TransformGroup)
		, BoneWeightAttribute(InCollection, BoneWeightAttributeName, FGeometryCollection::VerticesGroup, FTransformCollection::TransformGroup)
		, ParentAttribute(InCollection, FTransformCollection::ParentAttribute, FTransformCollection::TransformGroup)
		, VerticesAttribute(InCollection, "Vertex", FGeometryCollection::VerticesGroup)
	{
	}


	//
	//  Initialization
	//

	void FVertexBoneWeightsFacade::DefineSchema()
	{
		check(!IsConst());
		BoneIndexAttribute.Add();
		BoneWeightAttribute.Add();
		ParentAttribute.Add();
	}

	bool FVertexBoneWeightsFacade::IsValid() const
	{
		return BoneIndexAttribute.IsValid() && BoneWeightAttribute.IsValid() && ParentAttribute.IsValid() && VerticesAttribute.IsValid();
	}


	//
	//  Add Weights from a bone to a vertex 
	//
	void FVertexBoneWeightsFacade::AddBoneWeight(int32 VertexIndex, int32 BoneIndex, float BoneWeight)
	{
		TManagedArray< TArray<int32> >& IndicesArray = BoneIndexAttribute.Modify();
		TManagedArray< TArray<float> >& WeightsArray = BoneWeightAttribute.Modify();
		const TManagedArray<FVector3f>& Vertices = VerticesAttribute.Modify();
		if (0 <= VertexIndex && VertexIndex < Vertices.Num())
		{
			if (0 <= BoneIndex && BoneIndex < ParentAttribute.Num())
			{
				IndicesArray[VertexIndex].Add(BoneIndex);
				WeightsArray[VertexIndex].Add(BoneWeight);
			}
		}
	}


	//
	//  Add Weights from Selection 
	//

	void FVertexBoneWeightsFacade::AddBoneWeightsFromKinematicBindings() {
		check(!IsConst());
		DefineSchema();

		if (IsValid())
		{
			TArray<float> Weights;
			TArray<int32> Indices;

			int32 NumBones = ParentAttribute.Num(), NumVertices = BoneIndexAttribute.Num();
			TManagedArray< TArray<int32> >& IndicesArray = BoneIndexAttribute.Modify();
			TManagedArray< TArray<float> >& WeightsArray = BoneWeightAttribute.Modify();

			TArray<float> TotalWeights;
			TotalWeights.Init(0.f, WeightsArray.Num());

			for (int32 Vert = 0; Vert < WeightsArray.Num(); Vert++)
			{
				for (int32 i = 0; i < WeightsArray[Vert].Num(); i++)
				{
					TotalWeights[Vert] += WeightsArray[Vert][i];
				}
			}

			GeometryCollection::Facades::FKinematicBindingFacade BindingFacade(ConstCollection);
			for (int32 Kdx = BindingFacade.NumKinematicBindings() - 1; 0 <= Kdx; Kdx--)
			{
				int32 Bone;
				TArray<int32> OutBoneVerts;
				TArray<float> OutBoneWeights;

				BindingFacade.GetBoneBindings(BindingFacade.GetKinematicBindingKey(Kdx), Bone, OutBoneVerts, OutBoneWeights);

				if (0 <= Bone && Bone < NumBones)
				{
					for (int32 Vdx = 0; Vdx < OutBoneVerts.Num(); Vdx++)
					{
						int32 Vert = OutBoneVerts[Vdx]; float Weight = OutBoneWeights[Vdx];
						if (0 <= Vert && Vert < NumVertices && !IndicesArray[Vert].Contains(Bone))
						{
							int32 BoneIndex = IndicesArray[Vert].Find(Bone);
							if (TotalWeights[Vert] + Weight <= 1.f)
							{
								if (BoneIndex == INDEX_NONE)
								{
									IndicesArray[Vert].Add(Bone);
									WeightsArray[Vert].Add(Weight);
									TotalWeights[Vert] += Weight;
								}
								else if (0 <= BoneIndex && BoneIndex < WeightsArray[Vert].Num())
								{
									WeightsArray[Vert][BoneIndex] = Weight;
									TotalWeights[Vert] += Weight;
								}
							}
							else
							{
								UE_LOG(LogChaos, Warning, TEXT("Bone weight sum exceeds 1 on vertex %d"), Vert);
							}
						}
					}
				}
			}

		}
	}

};


