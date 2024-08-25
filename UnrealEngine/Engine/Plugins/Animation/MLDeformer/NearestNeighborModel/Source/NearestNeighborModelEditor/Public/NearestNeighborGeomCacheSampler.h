// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MLDeformerGeomCacheSampler.h"
#include "NearestNeighborModelHelpers.h"

class UAnimSequence;
class UGeometryCache;
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

		/** This will set the SkeletalMeshComponent with custom Anim and GeometryCacheComponent with custom Cache. 
		 * Using this fuction will stop using Anim and Cache from the EditorModel.
		 * Using this function will break Sample(). Call CustomSample() instead.
		 * Do NOT use this function on EditorModel Samplers. 
		 */ 
		void Customize(UAnimSequence* Anim, UGeometryCache* Cache = nullptr);

		/**
		 * Sample function used with Customize
		 * @return true if sampling is successful
		 */
		bool CustomSample(int32 Frame);

		/**
		 * Get the Mesh Index Buffer of the skeletal mesh object used by the model.
		 * 
		 * @return A flattened array of the index buffer.
		 */
		TArray<uint32> GetMeshIndexBuffer() const;

	private:
		void SampleDualQuaternionDeltas(int32 InAnimFrameIndex);
		EOpFlag GenerateMeshMappings();
		EOpFlag CheckMeshMappingsEmpty() const;
		FVector3f CalcDualQuaternionDelta(int32 VertexIndex, const FVector3f& WorldDelta, const FSkeletalMeshLODRenderData& SkelMeshLODData, const FSkinWeightVertexBuffer& SkinWeightBuffer) const;
	};
}
