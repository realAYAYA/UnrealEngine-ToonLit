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
		virtual uint8 SamplePart(int32 InAnimFrameIndex, int32 PartId);
		virtual bool SampleKMeansAnim(const int32 SkeletonId);
		virtual bool SampleKMeansFrame(const int32 Frame);
		const TArray<float>& GetPartVertexDeltas() const { return PartVertexDeltas; }
		uint8 GeneratePartMeshMappings(const TArray<uint32>& VertexMap, bool bUsePartOnlyMesh);
		uint8 GenerateMeshMappingIndices();
		uint8 CheckGeomCacheVertCount(int32 NumVertsFromGeomCache, int32 NumVertsFromVertexMap) const;
		uint8 CheckMeshMappingsEmpty() const;
		TArray<uint32> GetMeshIndexBuffer() const;
		FVector3f CalcDualQuaternionDelta(int32 VertexIndex, const FVector3f& WorldDelta, const FSkeletalMeshLODRenderData& SkelMeshLODData, const FSkinWeightVertexBuffer& SkinWeightBuffer) const;

		TArray<float> PartVertexDeltas;
		TArray<int32> MeshMappingIndices;
		int32 KMeansAnimId;
	};
}
