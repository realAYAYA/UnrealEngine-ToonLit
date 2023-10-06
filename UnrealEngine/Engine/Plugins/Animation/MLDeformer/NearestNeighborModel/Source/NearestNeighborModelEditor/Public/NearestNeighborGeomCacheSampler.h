// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MLDeformerGeomCacheSampler.h"

class UNearestNeighborTrainingModel;
namespace UE::NearestNeighborModel
{
	class FNearestNeighborEditorModel;

	class NEARESTNEIGHBORMODELEDITOR_API FNearestNeighborGeomCacheSampler
		: public UE::MLDeformer::FMLDeformerGeomCacheSampler
	{
	public:
		// FMLDeformerGeomCacheSampler overrides
		virtual void Sample(int32 InAnimFrameIndex) override;
		// ~END FMLDeformerGeomCacheSampler overrides

		friend class FNearestNeighborEditorModel;
		friend class ::UNearestNeighborTrainingModel;

	private:
		virtual void SampleDualQuaternionDeltas(int32 InAnimFrameIndex);
		virtual bool SampleKMeansAnim(const int32 SkeletonId);
		virtual bool SampleKMeansFrame(const int32 Frame);
		uint8 GenerateMeshMappings();
		uint8 CheckMeshMappingsEmpty() const;
		TArray<uint32> GetMeshIndexBuffer() const;
		FVector3f CalcDualQuaternionDelta(int32 VertexIndex, const FVector3f& WorldDelta, const FSkeletalMeshLODRenderData& SkelMeshLODData, const FSkinWeightVertexBuffer& SkinWeightBuffer) const;

		int32 KMeansAnimId;
	};
}
