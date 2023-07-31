// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerSampler.h"
#include "MLDeformerModel.h"
#include "MLDeformerGeomCacheHelpers.h"
#include "UObject/ObjectPtr.h"
#include "GeometryCacheMeshData.h"

class UGeometryCacheComponent;
class UGeometryCache;

namespace UE::MLDeformer
{
	DECLARE_DELEGATE_RetVal(UGeometryCache*, FMLDeformerGetGeomCacheEvent)

	/**
	 * The input data sampler, which is used to sample vertex positions from geometry caches.
	 * It can then also calculate deltas between the sampled skeletal mesh data and geometry cache data.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerGeomCacheSampler
		: public FMLDeformerSampler
	{
	public:
		// FVertexDeltaSampler overrides.
		virtual void Sample(int32 AnimFrameIndex) override;
		virtual void RegisterTargetComponents() override;
		virtual float GetTimeAtFrame(int32 InAnimFrameIndex) const override;
		// ~END FVertexDeltaSampler overrides.

		/**
		 * Get the array of mesh names that we cannot do any sampling for.
		 * The reason for this can be that the sampler cannot find a matching mesh in the skeletal mesh for specific geometry cache tracks.
		 * For example if there is some geometry cache track that is named "Head", but there is no such mesh inside the skeletal mesh, then
		 * the returned array of names will include a string with the value "Head".
		 * @return An array of geometry cache track names for which no mesh could be found inside the SkeletalMesh / linear skinned actor.
		 */
		const TArray<FString>& GetFailedImportedMeshNames() const	{ return FailedImportedMeshNames; }

		/**
		 * Executed when we need a pointer to the geometry cache object that we want to sample from.
		 * @return A reference to the delegate.
		 */
		FMLDeformerGetGeomCacheEvent& OnGetGeometryCache()			{ return GetGeometryCacheEvent; }

		UGeometryCacheComponent* GetGeometryCacheComponent()		{ return GeometryCacheComponent; }
		UGeometryCacheComponent* GetGeometryCacheComponent() const	{ return GeometryCacheComponent; }

		const TArray<FMLDeformerGeomCacheMeshMapping>& GetMeshMappings() const	{ return MeshMappings; }

	protected:
		/**
		 * Calculate the vertex deltas between the linear skinned mesh (skeletal mesh) and geometry cache.
		 * A delta cutoff length can be specified. Deltas with a length longer than this cutoff length will be ignored and set to zero.
		 * This can be useful in cases where for some reason due to some mesh errors some deltas are very long.
		 * Setting the DeltaCutoffLength to a very large value would essentially disable this filtering and always include all deltas.
		 * Typically a good value for the delta cutoff length seems to be 30.
		 * @param SkinnedPositions The vertex positions of the linear skinned mesh, so of the skeletal mesh.
		 * @param DeltaCutoffLength The delta cutoff length as described above. Usually a value of 30 is a good value, or set to a very large value to disable it.
		 * @param OutVertexDeltas The array that will receive the deltas, in a float buffer. The buffer will contain 3*NumVerts number of elements, and the layout is
		 *                        (x, y, z, x, y, z, x, y, z...) so 3 values per vertex: x, y and z.
		 */
		void CalculateVertexDeltas(const TArray<FVector3f>& SkinnedPositions, float DeltaCutoffLength, TArray<float>& OutVertexDeltas);

	protected:
		/** The geometry cache component used to sample the geometry cache. */
		TObjectPtr<UGeometryCacheComponent> GeometryCacheComponent = nullptr;

		/** Maps skeletal meshes imported meshes to geometry tracks. */
		TArray<FMLDeformerGeomCacheMeshMapping> MeshMappings;

		/** The geometry cache mesh data reusable buffers. One for each MeshMapping.*/
		TArray<FGeometryCacheMeshData> GeomCacheMeshDatas;

		/** Geom cache track names for which no mesh can be found inside the skeletal mesh. */
		TArray<FString> FailedImportedMeshNames; 

		/** Imported mesh names in the skeletal mesh for which the geometry track had a different vertex count. */
		TArray<FString> VertexCountMisMatchNames;

		/** The function that grabs the geometry cache. */
		FMLDeformerGetGeomCacheEvent GetGeometryCacheEvent;
	};
}	// namespace UE::MLDeformer
