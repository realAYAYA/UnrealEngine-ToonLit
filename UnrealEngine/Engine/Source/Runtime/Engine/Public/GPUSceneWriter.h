// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterMacros.h"
#include "ShaderParameters.h"

class FSceneView;
class FRDGBuilder;

/** Include in shader parameters struct for shaders that modify GPU-Scene instances */
BEGIN_SHADER_PARAMETER_STRUCT(FGPUSceneWriterParameters, ENGINE_API)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GPUSceneInstanceSceneDataRW)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GPUSceneInstancePayloadDataRW)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GPUScenePrimitiveSceneDataRW)
	SHADER_PARAMETER(uint32, GPUSceneInstanceSceneDataSOAStride)
	SHADER_PARAMETER(uint32, GPUSceneFrameNumber)
	SHADER_PARAMETER(uint32, GPUSceneNumAllocatedInstances)
	SHADER_PARAMETER(uint32, GPUSceneNumAllocatedPrimitives)
END_SHADER_PARAMETER_STRUCT()

/** Specifies the point during scene rendering that GPU Scene can be written to */
enum class EGPUSceneGPUWritePass : int8
{
	/** Invalid GPU write pass. Also used to signify writes that occur during upload. */
	None = -1,

	/** Writes to the GPU Scene will happen after rendering opaques (as well has after FXSystem post-opaque updates). */
	PostOpaqueRendering,

	Num
};

/** The parameters passed to the GPUScene writer delegate */
struct FGPUSceneWriteDelegateParams
{
	/** The ID of the primitive that writes must be limited to. */
	uint32 PersistentPrimitiveId = INDEX_NONE;
	/** The ID of the first instance scene data of the primitive */
	uint32 InstanceSceneDataOffset = INDEX_NONE;
	/** The GPU Scene write pass that is currently executing. (NOTE: A value of None specifies that it is occurring on upload) */
	EGPUSceneGPUWritePass GPUWritePass = EGPUSceneGPUWritePass::None;
	/** The view for which this primitive belongs (for dynamic primitives) */
	FSceneView* View = nullptr;
	/** The shader parameters the delegate can use to perform writes on GPU Scene data */
	FGPUSceneWriterParameters GPUWriteParams;
};

/** Delegate used by GPUScene to stage writing to the GPUScene primitive and instance data buffers via the GPU */
DECLARE_DELEGATE_TwoParams(FGPUSceneWriteDelegate, FRDGBuilder&, const FGPUSceneWriteDelegateParams&);