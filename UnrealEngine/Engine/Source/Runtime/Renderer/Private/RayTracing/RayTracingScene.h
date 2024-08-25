// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"

#if RHI_RAYTRACING

#include "Async/TaskGraphInterfaces.h"
#include "Math/DoubleFloat.h"
#include "RHI.h"
#include "RHIUtilities.h"
#include "RenderGraphResources.h"
#include "Misc/MemStack.h"
#include "Containers/ArrayView.h"
#include "MeshPassProcessor.h"
#include "RayTracingInstanceBufferUtil.h"
#include "RayTracingDebugTypes.h"

class FGPUScene;
class FRHIRayTracingScene;
class FRHIShaderResourceView;
class FRayTracingGeometry;
class FRDGBuilder;
class FPrimitiveSceneProxy;

namespace Nanite
{
	using CoarseMeshStreamingHandle = int16;
}

enum class ERayTracingSceneLayer : uint8
{
	Base,
	Decals,

	NUM
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

	uint32 AddInstance(FRayTracingGeometryInstance Instance, const FPrimitiveSceneProxy* Proxy = nullptr, bool bDynamic = false);

	uint32 AddInstancesUninitialized(uint32 NumInstances);

	void SetInstance(uint32 InstanceIndex, FRayTracingGeometryInstance Instance, const FPrimitiveSceneProxy* Proxy = nullptr, bool bDynamic = false);

	// Allocates RayTracingSceneRHI and builds various metadata required to create the final scene.
	FRayTracingSceneWithGeometryInstances BuildInitializationData() const;

	// Allocates GPU memory to fit at least the current number of instances.
	// Kicks off instance buffer build to parallel thread along with RDG pass.
	// NOTE: SceneWithGeometryInstances is passed in by value because ownership of the internal data is taken over. Use MoveTemp at call site, if possible.
	void CreateWithInitializationData(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FGPUScene* GPUScene, FRayTracingSceneWithGeometryInstances SceneWithGeometryInstances);

	// Backwards-compatible version of Create() which internally calls CreateRayTracingSceneWithGeometryInstances().
	void Create(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FGPUScene* GPUScene);

	// Resets the instance list and reserves memory for this frame.
	void Reset(bool bInstanceDebugDataEnabled);

	// Similar to Reset(), but also releases any persistent CPU and GPU memory allocations.
	void ResetAndReleaseResources();

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
	RENDERER_API FRDGBufferRef GetBufferChecked() const;

	// Creates new RHI view of a layer. Can only be used on valid ray tracing scene. 
	RENDERER_API FShaderResourceViewRHIRef CreateLayerViewRHI(FRHICommandListBase& RHICmdList, ERayTracingSceneLayer Layer) const;

	// Returns RDG view of a layer. Can only be used on valid ray tracing scene.
	RENDERER_API FRDGBufferSRVRef GetLayerView(ERayTracingSceneLayer Layer) const;

	TArrayView<const FRayTracingGeometryInstance> GetInstances() const { return MakeArrayView(Instances); }

	FRayTracingGeometryInstance& GetInstance(uint32 InstanceIndex) { return Instances[InstanceIndex]; }

	void InitPreViewTranslation(const FViewMatrices& ViewMatrices);

public:

	// Public members for initial refactoring step (previously were public members of FViewInfo).

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

	FRDGBufferRef DebugInstanceGPUSceneIndexBuffer = nullptr;
	bool bNeedsDebugInstanceGPUSceneIndexBuffer = false;

	// Used for transforming to translated world space in which TLAS was built.
	FDFVector3 PreViewTranslation {};
private:
	void WaitForTasks() const;

	// RHI object that abstracts mesh instnaces in this scene
	FRayTracingSceneRHIRef RayTracingSceneRHI;

	// Persistently allocated buffer that holds the built TLAS
	FBufferRHIRef RayTracingSceneBuffer;
	TRefCountPtr<FRDGPooledBuffer>	RayTracingScenePooledBuffer;		
	FRDGBufferRef RayTracingSceneBufferRDG;

	// Per-layer views for the TLAS buffer that should be used in ray tracing shaders
	TArray<FRDGBufferSRVRef> LayerSRVs;

	// Transient memory allocator
	FMemStackBase Allocator;

	FBufferRHIRef InstanceUploadBuffer;
	FShaderResourceViewRHIRef InstanceUploadSRV;

	FBufferRHIRef TransformUploadBuffer;
	FShaderResourceViewRHIRef TransformUploadSRV;

	FByteAddressBuffer AccelerationStructureAddressesBuffer;

	mutable FGraphEventRef FillInstanceUploadBufferTask;

	// Persistent storage for ray tracing instance descriptors.
	// Cleared every frame without releasing memory to avoid large heap allocations.
	// This must be filled before calling CreateRayTracingSceneWithGeometryInstances() and Create().
	TArray<FRayTracingGeometryInstance> Instances;

	TArray<FRayTracingInstanceDebugData> InstancesDebugData;

	bool bInstanceDebugDataEnabled = false;
};

#endif // RHI_RAYTRACING
