// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphDefinitions.h"
#include "CoreMinimal.h"

#if RHI_RAYTRACING

class FGPUScene;
struct FRayTracingCullingParameters;
struct FDFVector3;

/*
* 
* Each FRayTracingGeometryInstance can translate to multiple native TLAS instances (see FRayTracingGeometryInstance::NumTransforms).
* 
* The FRayTracingGeometryInstance array (ie: FRayTracingScene::Instances) used to create FRayTracingSceneRHI
* can have mix of instances using GPUScene, CPU transforms or GPUTransformSRV.
* In order to reduce the number of dispatches to build the native RayTracing Instance Buffer,
* the upload buffer containing FRayTracingInstanceDescriptorInput is split in 3 sections, [GPUSceneInstances] [CPUInstances] [GPUInstances].
* This way native GPUScene and CPU instance descriptors can be built in a single dispatch per type.
* Followed by one dispatch per GPU instance (since GPU transforms of each GPU instance are stored in separate buffers).
* 
* If the raytracing scene contains multiple layers, the instance buffer is divided into multiple subranges as expected by the RHI.
* 
*/

struct FRayTracingInstanceDescriptorInput
{
	uint32 GPUSceneInstanceOrTransformIndex;
	uint32 OutputDescriptorIndex;
	uint32 AccelerationStructureIndex;
	uint32 InstanceId;
	uint32 InstanceMaskAndFlags;
	uint32 InstanceContributionToHitGroupIndex;
	uint32 bApplyLocalBoundsTransform;
};

struct FRayTracingGPUInstance
{
	FShaderResourceViewRHIRef TransformSRV;
	uint32 NumInstances;
	uint32 DescBufferOffset;
};

struct FRayTracingSceneWithGeometryInstances
{
	FRayTracingSceneRHIRef Scene;
	uint32 NumNativeGPUSceneInstances;
	uint32 NumNativeCPUInstances;
	uint32 NumNativeGPUInstances;
	// index of each instance geometry in FRayTracingSceneRHIRef ReferencedGeometries
	TArray<uint32> InstanceGeometryIndices;
	// base offset of each instance entries in the instance upload buffer
	TArray<uint32> BaseUploadBufferOffsets;
	TArray<FRayTracingGPUInstance> GPUInstances;
};

// Helper function to create FRayTracingSceneRHI using array of high level instances
// Also outputs data required to build the instance buffer
RENDERER_API FRayTracingSceneWithGeometryInstances CreateRayTracingSceneWithGeometryInstances(
	TConstArrayView<FRayTracingGeometryInstance> Instances,
	uint8 NumLayers,
	uint32 NumShaderSlotsPerGeometrySegment,
	uint32 NumMissShaderSlots,
	uint32 NumCallableShaderSlots = 0);

UE_DEPRECATED(5.1, "Specify NumLayers instead.")
RENDERER_API FRayTracingSceneWithGeometryInstances CreateRayTracingSceneWithGeometryInstances(
	TArrayView<FRayTracingGeometryInstance> Instances,
	uint32 NumShaderSlotsPerGeometrySegment,
	uint32 NumMissShaderSlots);

// Helper function to fill upload buffers required by BuildRayTracingInstanceBuffer with instance descriptors
// Transforms of CPU instances are copied to OutTransformData
RENDERER_API void FillRayTracingInstanceUploadBuffer(
	FRayTracingSceneRHIRef RayTracingSceneRHI,
	FVector PreViewTranslation,
	TConstArrayView<FRayTracingGeometryInstance> Instances,
	TConstArrayView<uint32> InstanceGeometryIndices,
	TConstArrayView<uint32> BaseUploadBufferOffsets,
	uint32 NumNativeGPUSceneInstances,
	uint32 NumNativeCPUInstances,
	TArrayView<FRayTracingInstanceDescriptorInput> OutInstanceUploadData,
	TArrayView<FVector4f> OutTransformData);

RENDERER_API void BuildRayTracingInstanceBuffer(
	FRHICommandList& RHICmdList,
	const FGPUScene* GPUScene,
	const FDFVector3& PreViewTranslation,
	FUnorderedAccessViewRHIRef InstancesUAV,
	FShaderResourceViewRHIRef InstanceUploadSRV,
	FShaderResourceViewRHIRef AccelerationStructureAddressesSRV,
	FShaderResourceViewRHIRef CPUInstanceTransformSRV,
	uint32 NumNativeGPUSceneInstances,
	uint32 NumNativeCPUInstances,
	TConstArrayView<FRayTracingGPUInstance> GPUInstances,
	const FRayTracingCullingParameters* CullingParameters,
	FUnorderedAccessViewRHIRef DebugInstanceGPUSceneIndexUAV);

#endif // RHI_RAYTRACING