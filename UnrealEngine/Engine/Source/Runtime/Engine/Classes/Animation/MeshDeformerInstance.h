// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshDeformerInstance.generated.h"

class FSceneInterface;


/**
 * Base class for mesh deformers instance settings.
 * This contains the serialized user settings to apply to the UMeshDeformer.
 */
UCLASS(Abstract, MinimalAPI)
class UMeshDeformerInstanceSettings : public UObject
{
	GENERATED_BODY()
};


/** 
 * Base class for mesh deformers instances.
 * This contains the transient per instance state for a UMeshDeformer.
 */
UCLASS(Abstract, MinimalAPI)
class UMeshDeformerInstance : public UObject
{
	GENERATED_BODY()

public:
	/** Called to allocate any persistent render resources */
	ENGINE_API virtual void AllocateResources() PURE_VIRTUAL(, );

	/** Called when persistent render resources should be released */
	ENGINE_API virtual void ReleaseResources() PURE_VIRTUAL(, );

	/** Enumeration for workloads to EnqueueWork. */
	enum EWorkLoad
	{
		WorkLoad_Setup,
		WorkLoad_Trigger,
		WorkLoad_Update,
	};

	/** Enumeration for execution groups to EnqueueWork on. */
	enum EExectutionGroup
	{
		ExecutionGroup_Default,
		ExecutionGroup_Immediate,
		ExecutionGroup_EndOfFrameUodate,
	};

	/** Structure of inputs to EnqueueWork. */
	struct FEnqueueWorkDesc
	{
		FSceneInterface* Scene = nullptr;
		EWorkLoad WorkLoadType = WorkLoad_Update;
		EExectutionGroup ExecutionGroup = ExecutionGroup_Default;
		/** Name used for debugging and profiling markers. */
		FName OwnerName;
		/** Render thread delegate that will be executed if Enqueue fails at any stage. */
		FSimpleDelegate FallbackDelegate;
	};

	/** Enqueue the mesh deformer workload on a scene. */
	ENGINE_API virtual void EnqueueWork(FEnqueueWorkDesc const& InDesc) PURE_VIRTUAL(, );
};
