// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingValidationShaders.h"
#include "ShaderParameterUtils.h"
#include "PipelineStateCache.h"

#if RHI_RAYTRACING

// FRayTracingValidateGeometryBuildParamsCS

IMPLEMENT_GLOBAL_SHADER(FRayTracingValidateGeometryBuildParamsCS, "/Engine/Private/RayTracing/RayTracingValidation.usf", "RayTracingValidateGeometryBuildParamsCS", SF_Compute);

void FRayTracingValidateGeometryBuildParamsCS::Dispatch(FRHICommandList& RHICmdList, const FRayTracingGeometryBuildParams& Params)
{
	const FRayTracingGeometryInitializer& Initializer = Params.Geometry->GetInitializer();

	TShaderMapRef<FRayTracingValidateGeometryBuildParamsCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
	SetComputePipelineState(RHICmdList, ShaderRHI);

	// TODO: handle non-indexed geometry
	if (Initializer.IndexBuffer == nullptr)
	{
		return;
	}

	TWideStringBuilder<256> EventName;
	EventName.Append(TEXT("RTGeometryValidation"));
	if (!Initializer.DebugName.IsNone())
	{
		FString DebugNameString = Initializer.DebugName.ToString();
		EventName.Append(TEXT(" - "));
		EventName.Append(*DebugNameString);
	}

	RHICmdList.PushEvent(EventName.ToString(), FColor::Black);

	const uint32 IndexStride = Initializer.IndexBuffer->GetStride();

	const FRawBufferShaderResourceViewInitializer IBViewInitializer(Initializer.IndexBuffer);
	FShaderResourceViewRHIRef IndexBufferSRV = RHICreateShaderResourceView(IBViewInitializer);

	for (const FRayTracingGeometrySegment& Segment : Initializer.Segments)
	{
		if (Segment.VertexBufferElementType != VET_Float3)
		{
			// Only Float3 vertex positions are currently supported
			continue;
		}

		const uint32 IndexBufferOffsetInBytes = Segment.FirstPrimitive * IndexStride * 3;

		SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->VertexBufferStrideParam, Segment.VertexBufferStride);
		SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->VertexBufferOffsetInBytesParam, Segment.VertexBufferOffset);
		SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->IndexBufferOffsetInBytesParam, IndexBufferOffsetInBytes);
		SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->IndexBufferStrideParam, IndexStride);
		SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->NumPrimitivesParam, Segment.NumPrimitives);
		SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->MaxVerticesParam, Segment.MaxVertices);

		const FRawBufferShaderResourceViewInitializer VBViewInitializer(Segment.VertexBuffer);
		FShaderResourceViewRHIRef VertexBufferSRV = RHICreateShaderResourceView(VBViewInitializer);

		SetSRVParameter(RHICmdList, ShaderRHI, ComputeShader->VertexBufferParam, VertexBufferSRV);
		SetSRVParameter(RHICmdList, ShaderRHI, ComputeShader->IndexBufferParam, IndexBufferSRV);

		// TODO: handle arbitrary large meshes that may overrun the 1D dispatch limit
		const uint32 MaxDispatchDimension = 65536; // D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION
		const uint32 NumGroupsX = FMath::Min((Segment.NumPrimitives + NumThreadsX - 1) / NumThreadsX, MaxDispatchDimension);

		RHICmdList.DispatchComputeShader(NumGroupsX, 1, 1);
	}

	SetSRVParameter(RHICmdList, ShaderRHI, ComputeShader->VertexBufferParam, nullptr);
	SetSRVParameter(RHICmdList, ShaderRHI, ComputeShader->IndexBufferParam, nullptr);

	RHICmdList.PopEvent();
}

// FRayTracingValidateSceneBuildParamsCS

IMPLEMENT_GLOBAL_SHADER(FRayTracingValidateSceneBuildParamsCS, "/Engine/Private/RayTracing/RayTracingValidation.usf", "RayTracingValidateSceneBuildParamsCS", SF_Compute);

void FRayTracingValidateSceneBuildParamsCS::Dispatch(FRHICommandList& RHICmdList, 
	uint32 NumHitGroups, uint32 NumInstances, 
	FRHIBuffer* InstanceBuffer, uint32 InstanceBufferOffset, uint32 InstanceBufferStride)
{
	TShaderMapRef<FRayTracingValidateSceneBuildParamsCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
	SetComputePipelineState(RHICmdList, ShaderRHI);

	RHICmdList.PushEvent(TEXT("RTSceneValidation"), FColor::Black);

	const FRawBufferShaderResourceViewInitializer InstanceBufferViewInitializer(InstanceBuffer);
	FShaderResourceViewRHIRef InstanceBufferSRV = RHICreateShaderResourceView(InstanceBufferViewInitializer);

	SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->NumInstancesParam, NumInstances);
	SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->NumHitGroupsParam, NumHitGroups);
	SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->InstanceBufferOffsetInBytesParam, InstanceBufferOffset);
	SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->InstanceBufferStrideInBytesParam, InstanceBufferStride);
	SetSRVParameter(RHICmdList, ShaderRHI, ComputeShader->InstanceBufferParam, InstanceBufferSRV);

	const uint32 MaxDispatchDimension = 65536; // D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION
	const uint32 NumGroupsX = FMath::Min((NumInstances + NumThreadsX - 1) / NumThreadsX, MaxDispatchDimension);

	RHICmdList.DispatchComputeShader(NumGroupsX, 1, 1);

	SetSRVParameter(RHICmdList, ShaderRHI, ComputeShader->InstanceBufferParam, nullptr);

	RHICmdList.PopEvent();
}

#endif // RHI_RAYTRACING
