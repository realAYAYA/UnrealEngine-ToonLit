// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
CustomizableObjectMeshUpdate.h: Helpers to stream in skeletal mesh LODs.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Streaming/SkeletalMeshUpdate.h"

#include "MuR/System.h"
#include "MuR/Mesh.h"

class USkeletalMesh;

namespace mu 
{ 
	class Parameters;
}

struct FMutableMeshOperationData
{
	/** Mutable */
	mu::Ptr<mu::System> System;
	TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model;
	mu::Ptr<const mu::Parameters> Parameters;
	int32 State = -1;

	// IDs of the meshes to generate per LOD.
	TArray<mu::FResourceID> MeshIDs;

	// Meshes generated per LOD
	TArray<mu::MeshPtrConst> Meshes;

	/** SkeletalMesh */
	/** The resident first LOD resource index.With domain = [0, ResourceState.NumLODs[.NOT THE ASSET LOD INDEX! */ 
	int32 CurrentFirstLODIdx = INDEX_NONE;

	/** The requested first LOD resource index. With domain = [0, ResourceState.NumLODs[. NOT THE ASSET LOD INDEX! */
	int32 PendingFirstLODIdx = INDEX_NONE;
};

extern template class TRenderAssetUpdate<FSkelMeshUpdateContext>;

/** 
* This class provides a framework for loading the LODs of CustomizableObject skeletal meshes.
* Each thread essentially calls Tick() until the job is done.
* The object can be safely deleted when IsCompleted() returns true.
*/
class FCustomizableObjectMeshStreamIn : public FSkeletalMeshStreamIn
{
public:
	FCustomizableObjectMeshStreamIn(const USkeletalMesh* InMesh, bool bHighPrio, bool bRenderThread);

	void OnUpdateMeshFinished();

private:

	void DoInitiate(const FContext& Context);

	void DoConvertResources(const FContext& Context);

	void DoCreateBuffers(const FContext& Context);

	void DoCancelMeshUpdate(const FContext& Context);

	/** Creates a MeshUpdate task to generate the meshes for the LODs to stream in. */
	void RequestMeshUpdate(const FContext& Context);

	/** Cancels the MeshUpdate task. */
	void CancelMeshUpdate(const FContext& Context);

	/** Converts from mu::Mesh to FSkeletalMeshLODRenderData. */
	void ConvertMesh(const FContext& Context);

	/** TODO: Stream priority. */
	bool bHighPriority = false;
	
	/** Used to create the buffers in the render thread or not. */
	bool bRenderThread = false;

	/** Context of the MeshUpdate. */
	TSharedPtr<FMutableMeshOperationData> OperationData;

	/** MeshUpdate task Id to cancel the task if the stream in task is aborted. */
	uint32 MutableTaskId = 0;
};
