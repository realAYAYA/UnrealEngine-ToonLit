// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshOperations.h"

#include "BoneWeights.h"
#include "MeshDescriptionAdapter.h"
#include "SkeletalMeshAttributes.h"
#include "Spatial/MeshAABBTree3.h"


DEFINE_LOG_CATEGORY(LogSkeletalMeshOperations);

#define LOCTEXT_NAMESPACE "SkeletalMeshOperations"

namespace UE::Private
{
	template<typename T>
	struct FCreateAndCopyAttributeValues
	{
		FCreateAndCopyAttributeValues(
			const FMeshDescription& InSourceMesh,
			FMeshDescription& InTargetMesh,
			TArray<FName>& InTargetCustomAttributeNames,
			int32 InTargetVertexIndexOffset)
			: SourceMesh(InSourceMesh)
			, TargetMesh(InTargetMesh)
			, TargetCustomAttributeNames(InTargetCustomAttributeNames)
			, TargetVertexIndexOffset(InTargetVertexIndexOffset)
			{}

		void operator()(const FName InAttributeName, TVertexAttributesConstRef<T> InSrcAttribute)
		{
			// Ignore attributes with reserved names.
			if (FSkeletalMeshAttributes::IsReservedAttributeName(InAttributeName))
			{
				return;
			}
			TAttributesSet<FVertexID>& VertexAttributes = TargetMesh.VertexAttributes();
			const bool bAppend = TargetCustomAttributeNames.Contains(InAttributeName);
			if (!bAppend)
			{
				VertexAttributes.RegisterAttribute<T>(InAttributeName, InSrcAttribute.GetNumChannels(), InSrcAttribute.GetDefaultValue(), InSrcAttribute.GetFlags());
				TargetCustomAttributeNames.Add(InAttributeName);
			}
			//Copy the data
			TVertexAttributesRef<T> TargetVertexAttributes = VertexAttributes.GetAttributesRef<T>(InAttributeName);
			for (const FVertexID SourceVertexID : SourceMesh.Vertices().GetElementIDs())
			{
				const FVertexID TargetVertexID = FVertexID(TargetVertexIndexOffset + SourceVertexID.GetValue());
				TargetVertexAttributes.Set(TargetVertexID, InSrcAttribute.Get(SourceVertexID));
			}
		}

		// Unhandled sub-types.
		void operator()(const FName, TVertexAttributesConstRef<TArrayAttribute<T>>) { }
		void operator()(const FName, TVertexAttributesConstRef<TArrayView<T>>) { }

	private:
		const FMeshDescription& SourceMesh;
		FMeshDescription& TargetMesh;
		TArray<FName>& TargetCustomAttributeNames;
		int32 TargetVertexIndexOffset = 0;
	};

} // ns UE::Private

//Add specific skeletal mesh descriptions implementation here
void FSkeletalMeshOperations::AppendSkinWeight(const FMeshDescription& SourceMesh, FMeshDescription& TargetMesh, FSkeletalMeshAppendSettings& AppendSettings)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FSkeletalMeshOperations::AppendSkinWeight");
	FSkeletalMeshConstAttributes SourceSkeletalMeshAttributes(SourceMesh);
	
	FSkeletalMeshAttributes TargetSkeletalMeshAttributes(TargetMesh);
	constexpr bool bKeepExistingAttribute = true;
	TargetSkeletalMeshAttributes.Register(bKeepExistingAttribute);
	
	FSkinWeightsVertexAttributesConstRef SourceVertexSkinWeights = SourceSkeletalMeshAttributes.GetVertexSkinWeights();
	FSkinWeightsVertexAttributesRef TargetVertexSkinWeights = TargetSkeletalMeshAttributes.GetVertexSkinWeights();

	TargetMesh.SuspendVertexIndexing();
	
	//Append Custom VertexAttribute
	if(AppendSettings.bAppendVertexAttributes)
	{
		TArray<FName> TargetCustomAttributeNames;
		TargetMesh.VertexAttributes().GetAttributeNames(TargetCustomAttributeNames);
		int32 TargetVertexIndexOffset = FMath::Max(TargetMesh.Vertices().Num() - SourceMesh.Vertices().Num(), 0);

		SourceMesh.VertexAttributes().ForEachByType<float>(UE::Private::FCreateAndCopyAttributeValues<float>(SourceMesh, TargetMesh, TargetCustomAttributeNames, AppendSettings.SourceVertexIDOffset));
		SourceMesh.VertexAttributes().ForEachByType<FVector2f>(UE::Private::FCreateAndCopyAttributeValues<FVector2f>(SourceMesh, TargetMesh, TargetCustomAttributeNames, AppendSettings.SourceVertexIDOffset));
		SourceMesh.VertexAttributes().ForEachByType<FVector3f>(UE::Private::FCreateAndCopyAttributeValues<FVector3f>(SourceMesh, TargetMesh, TargetCustomAttributeNames, AppendSettings.SourceVertexIDOffset));
		SourceMesh.VertexAttributes().ForEachByType<FVector4f>(UE::Private::FCreateAndCopyAttributeValues<FVector4f>(SourceMesh, TargetMesh, TargetCustomAttributeNames, AppendSettings.SourceVertexIDOffset));
	}

	for (const FVertexID SourceVertexID : SourceMesh.Vertices().GetElementIDs())
	{
		const FVertexID TargetVertexID = FVertexID(AppendSettings.SourceVertexIDOffset + SourceVertexID.GetValue());
		FVertexBoneWeightsConst SourceBoneWeights = SourceVertexSkinWeights.Get(SourceVertexID);
		TArray<UE::AnimationCore::FBoneWeight> TargetBoneWeights;
		const int32 InfluenceCount = SourceBoneWeights.Num();
		for (int32 InfluenceIndex = 0; InfluenceIndex < InfluenceCount; ++InfluenceIndex)
		{
			const FBoneIndexType SourceBoneIndex = SourceBoneWeights[InfluenceIndex].GetBoneIndex();
			if(AppendSettings.SourceRemapBoneIndex.IsValidIndex(SourceBoneIndex))
			{
				UE::AnimationCore::FBoneWeight& TargetBoneWeight = TargetBoneWeights.AddDefaulted_GetRef();
				TargetBoneWeight.SetBoneIndex(AppendSettings.SourceRemapBoneIndex[SourceBoneIndex]);
				TargetBoneWeight.SetRawWeight(SourceBoneWeights[InfluenceIndex].GetRawWeight());
			}
		}
		TargetVertexSkinWeights.Set(TargetVertexID, TargetBoneWeights);
	}

	TargetMesh.ResumeVertexIndexing();
}


bool FSkeletalMeshOperations::CopySkinWeightAttributeFromMesh(
	const FMeshDescription& InSourceMesh,
	FMeshDescription& InTargetMesh,
	const FName InSourceProfile,
	const FName InTargetProfile,
	const TMap<int32, int32>* SourceBoneIndexToTargetBoneIndexMap
	)
{
	// This is effectively a slower and dumber version of FTransferBoneWeights.
	using namespace UE::AnimationCore;
	using namespace UE::Geometry;

	FSkeletalMeshConstAttributes SourceAttributes(InSourceMesh);
	FSkeletalMeshAttributes TargetAttributes(InTargetMesh);
	
	FSkinWeightsVertexAttributesConstRef SourceWeights = SourceAttributes.GetVertexSkinWeights(InSourceProfile);
	FSkinWeightsVertexAttributesRef TargetWeights = TargetAttributes.GetVertexSkinWeights(InTargetProfile);
	TVertexAttributesConstRef<FVector3f> TargetPositions = TargetAttributes.GetVertexPositions();

	if (!SourceWeights.IsValid() || !TargetWeights.IsValid())
	{
		return false;
	}
	
	FMeshDescriptionTriangleMeshAdapter MeshAdapter(&InSourceMesh);
	TMeshAABBTree3<FMeshDescriptionTriangleMeshAdapter> BVH(&MeshAdapter);

	auto RemapBoneWeights = [SourceBoneIndexToTargetBoneIndexMap](const FVertexBoneWeightsConst& InWeights) -> FBoneWeights
	{
		TArray<FBoneWeight, TInlineAllocator<MaxInlineBoneWeightCount>> Weights;

		if (SourceBoneIndexToTargetBoneIndexMap)
		{
			for (FBoneWeight OriginalWeight: InWeights)
			{
				if (const int32* BoneIndexPtr = SourceBoneIndexToTargetBoneIndexMap->Find(OriginalWeight.GetBoneIndex()))
				{
					FBoneWeight NewWeight(static_cast<FBoneIndexType>(*BoneIndexPtr), OriginalWeight.GetRawWeight());
					Weights.Add(NewWeight);
				}
			}

			if (Weights.IsEmpty())
			{
				const FBoneWeight RootBoneWeight(0, 1.0f);
				Weights.Add(RootBoneWeight);
			}
		}
		else
		{
			for (FBoneWeight Weight: InWeights)
			{
				Weights.Add(Weight);
			}
		}
		return FBoneWeights::Create(Weights);
	};
	
	auto InterpolateWeights = [&MeshAdapter, &SourceWeights, &RemapBoneWeights](int32 InTriangleIndex, const FVector3d& InTargetPoint) -> FBoneWeights
	{
		const FDistPoint3Triangle3d Query = TMeshQueries<FMeshDescriptionTriangleMeshAdapter>::TriangleDistance(MeshAdapter, InTriangleIndex, InTargetPoint);

		const FIndex3i TriangleVertexes = MeshAdapter.GetTriangle(InTriangleIndex);
		const FVector3f BaryCoords(VectorUtil::BarycentricCoords(Query.ClosestTrianglePoint, MeshAdapter.GetVertex(TriangleVertexes.A), MeshAdapter.GetVertex(TriangleVertexes.B), MeshAdapter.GetVertex(TriangleVertexes.C)));
		const FBoneWeights WeightsA = RemapBoneWeights(SourceWeights.Get(TriangleVertexes.A));
		const FBoneWeights WeightsB = RemapBoneWeights(SourceWeights.Get(TriangleVertexes.B));
		const FBoneWeights WeightsC = RemapBoneWeights(SourceWeights.Get(TriangleVertexes.C));

		FBoneWeights BoneWeights = FBoneWeights::Blend(WeightsA, WeightsB, WeightsC, BaryCoords.X, BaryCoords.Y, BaryCoords.Z);
		
		// Blending can leave us with zero weights. Let's strip them out here.
		BoneWeights.Renormalize();
		return BoneWeights;
	};

	TArray<FBoneWeights> TargetBoneWeights;
	TargetBoneWeights.SetNum(InTargetMesh.Vertices().GetArraySize());

	ParallelFor(InTargetMesh.Vertices().GetArraySize(), [&BVH, &InTargetMesh, &TargetPositions, &TargetBoneWeights, &InterpolateWeights](int32 InVertexIndex)
	{
		const FVertexID VertexID(InVertexIndex);
		if (!InTargetMesh.Vertices().IsValid(VertexID))
		{
			return;
		}

		const FVector3d TargetPoint(TargetPositions.Get(VertexID));

		const IMeshSpatial::FQueryOptions Options;
		double NearestDistanceSquared;
		const int32 NearestTriangleIndex = BVH.FindNearestTriangle(TargetPoint, NearestDistanceSquared, Options);

		if (!ensure(NearestTriangleIndex != IndexConstants::InvalidID))
		{
			return;
		}

		TargetBoneWeights[InVertexIndex] = InterpolateWeights(NearestTriangleIndex, TargetPoint);
	});

	// Transfer the computed bone weights to the target mesh.
	for (FVertexID TargetVertexID: InTargetMesh.Vertices().GetElementIDs())
	{
		FBoneWeights& BoneWeights = TargetBoneWeights[TargetVertexID];
		if (BoneWeights.Num() == 0)
		{
			// Bind to root so that we have something.
			BoneWeights.SetBoneWeight(FBoneIndexType{0}, 1.0);
		}

		TargetWeights.Set(TargetVertexID, BoneWeights);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
