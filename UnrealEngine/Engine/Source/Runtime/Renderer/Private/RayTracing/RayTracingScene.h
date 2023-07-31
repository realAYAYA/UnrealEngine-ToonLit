// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"

#if RHI_RAYTRACING

#include "Async/TaskGraphInterfaces.h"
#include "RHI.h"
#include "RenderGraphResources.h"
#include "Misc/MemStack.h"
#include "Containers/ArrayView.h"
#include "PrimitiveSceneProxy.h"
#include "MeshPassProcessor.h"
#include "RayTracingInstanceBufferUtil.h"
#include "RayTracingDebugTypes.h"

class FGPUScene;
class FRHIRayTracingScene;
class FRHIShaderResourceView;
class FRayTracingGeometry;
class FRDGBuilder;

enum class ERayTracingSceneLayer : uint8
{
	Base,

	NUM
};

enum class ERayTracingSceneState
{
	Writable, // Scene is being built and can't be bound as SRV
	Readable, // Scene can be used in ray tracing commands (can be bound as SRV)
};

/**
* Persistent representation of the scene for ray tracing.
* Manages top level acceleration structure instances, memory and build process.
*/
class FRayTracingScene
{
public:

	FRayTracingScene();
	~FRayTracingScene();

	// Allocates RayTracingSceneRHI and builds various metadata required to create the final scene.
	FRayTracingSceneWithGeometryInstances BuildInitializationData() const;

	// Allocates GPU memory to fit at least the current number of instances.
	// Kicks off instance buffer build to parallel thread along with RDG pass.
	// NOTE: SceneWithGeometryInstances is passed in by value because ownership of the internal data is taken over. Use MoveTemp at call site, if possible.
	void CreateWithInitializationData(FRDGBuilder& GraphBuilder, const FGPUScene* GPUScene, const FViewMatrices& ViewMatrices, FRayTracingSceneWithGeometryInstances SceneWithGeometryInstances);

	// Backwards-compatible version of Create() which internally calls CreateRayTracingSceneWithGeometryInstances().
	void Create(FRDGBuilder& GraphBuilder, const FGPUScene* GPUScene, const FViewMatrices& ViewMatrices);

	// Resets the instance list and reserves memory for this frame.
	void Reset();

	// Similar to Reset(), but also releases any persistent CPU and GPU memory allocations.
	void ResetAndReleaseResources();

	void Transition(FRDGBuilder& GraphBuilder, ERayTracingSceneState InState);

	// Allocates temporary memory that will be valid until the next Reset().
	// Can be used to store temporary instance transforms, user data, etc.
	template <typename T>
	TArrayView<T> Allocate(int32 Count)
	{
		return MakeArrayView(new(Allocator) T[Count], Count);
	}

	// Returns true if RHI ray tracing scene has been created.
	// i.e. returns true after BeginCreate() and before Reset().
	RENDERER_API bool IsCreated() const;

	// Returns RayTracingSceneRHI object (may return null).
	RENDERER_API  FRHIRayTracingScene* GetRHIRayTracingScene() const;

	// Similar to GetRayTracingScene, but checks that ray tracing scene RHI object is valid.
	RENDERER_API  FRHIRayTracingScene* GetRHIRayTracingSceneChecked() const;

	// Returns Buffer for this ray tracing scene.
	// Valid to call immediately after Create() and does not block.
	RENDERER_API FRHIBuffer* GetBufferChecked() const;
	
	RENDERER_API FRHIShaderResourceView* GetLayerSRVChecked(ERayTracingSceneLayer Layer) const;
	
	void AddInstanceDebugData(const FRHIRayTracingGeometry* GeometryRHI, const FPrimitiveSceneProxy* Proxy, bool bDynamic);

	void InitPreViewTranslation(const FViewMatrices& ViewMatrices);
public:

	// Public members for initial refactoring step (previously were public members of FViewInfo).

	// Persistent storage for ray tracing instance descriptors.
	// Cleared every frame without releasing memory to avoid large heap allocations.
	// This must be filled before calling CreateRayTracingSceneWithGeometryInstances() and Create().
	TArray<FRayTracingGeometryInstance> Instances;

	bool bUsedThisFrame = false;
	uint32 NumMissShaderSlots = 1; // we must have a default miss shader, so always include it from the start
	uint32 NumCallableShaderSlots = 0;
	TArray<FRayTracingShaderCommand> CallableCommands;

	// Helper array to hold references to single frame uniform buffers used in SBTs
	TArray<FUniformBufferRHIRef> UniformBuffers;

	// Geometries which still have a pending build request but are used this frame and require a force build.
	TArray<const FRayTracingGeometry*> GeometriesToBuild;

	// Used coarse mesh streaming handles during the last TLAS build
	TArray<Nanite::CoarseMeshStreamingHandle> UsedCoarseMeshStreamingHandles;

	FRDGBufferRef InstanceBuffer;
	FRDGBufferRef BuildScratchBuffer;

	// Special data for debugging purposes
	FRDGBufferRef InstanceDebugBuffer = nullptr;
	TArray<FRayTracingInstanceDebugData> InstancesDebugData;

	// Used for transforming to translated world space in which TLAS was built.
	FVector RelativePreViewTranslation = FVector::Zero();
	FVector3f ViewTilePosition = FVector3f::ZeroVector;
private:
	void WaitForTasks() const;

	// RHI object that abstracts mesh instnaces in this scene
	FRayTracingSceneRHIRef RayTracingSceneRHI;

	// Persistently allocated buffer that holds the built TLAS
	FBufferRHIRef RayTracingSceneBuffer;

	// Per-layer views for the TLAS buffer that should be used in ray tracing shaders
	TArray<FShaderResourceViewRHIRef> LayerSRVs;

	// Transient memory allocator
	FMemStackBase Allocator;

	FBufferRHIRef InstanceUploadBuffer;
	FShaderResourceViewRHIRef InstanceUploadSRV;

	FBufferRHIRef TransformUploadBuffer;
	FShaderResourceViewRHIRef TransformUploadSRV;

	FByteAddressBuffer AccelerationStructureAddressesBuffer;

	mutable FGraphEventRef FillInstanceUploadBufferTask;

	ERayTracingSceneState State = ERayTracingSceneState::Writable;
};

#endif // RHI_RAYTRACING
