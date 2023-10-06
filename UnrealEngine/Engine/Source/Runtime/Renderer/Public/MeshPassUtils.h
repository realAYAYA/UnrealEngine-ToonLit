// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshPassProcessor.h"
#include "RenderGraphUtils.h"

namespace UE::MeshPassUtils
{
	namespace Private
	{
		template<typename TShaderClass>
		inline void PrepareDispatch(
			FRHIComputeCommandList& RHICmdList,
			const TShaderRef<TShaderClass>& ComputeShader,
			const FMeshDrawShaderBindings& ShaderBindings,
			const typename TShaderClass::FParameters& PassParameters)
		{
			FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();

			FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
			ShaderBindings.SetParameters(BatchedParameters, ShaderRHI);
			SetShaderParameters(BatchedParameters, ComputeShader, PassParameters);

			SetComputePipelineState(RHICmdList, ShaderRHI);
			RHICmdList.SetBatchedShaderParameters(ShaderRHI, BatchedParameters);
		}

		template<typename TShaderClass>
		inline void AfterDispatch(FRHIComputeCommandList& RHICmdList, const TShaderRef<TShaderClass>& ComputeShader)
		{
			UnsetShaderUAVs(RHICmdList, ComputeShader, ComputeShader.GetComputeShader());
		}
	}

	/** Dispatch a compute shader to RHI Command List with its parameters and mesh shader bindings. */
	template<typename TShaderClass>
	inline void Dispatch(
		FRHIComputeCommandList& RHICmdList,
		const TShaderRef<TShaderClass>& ComputeShader,
		const FMeshDrawShaderBindings& ShaderBindings,
		const typename TShaderClass::FParameters& PassParameters,
		FIntVector GroupCount)
	{
		FComputeShaderUtils::ValidateGroupCount(GroupCount);

		Private::PrepareDispatch(RHICmdList, ComputeShader, ShaderBindings, PassParameters);
		RHICmdList.DispatchComputeShader(GroupCount.X, GroupCount.Y, GroupCount.Z);
		Private::AfterDispatch(RHICmdList, ComputeShader);
	}

	/** Indirect dispatch a compute shader to RHI Command List with its parameters and mesh shader bindings. */
	template<typename TShaderClass>
	inline void DispatchIndirect(
		FRHIComputeCommandList& RHICmdList,
		const TShaderRef<TShaderClass>& ComputeShader,
		const FMeshDrawShaderBindings& ShaderBindings,
		const typename TShaderClass::FParameters& PassParameters,
		FRHIBuffer* IndirectArgsBuffer,
		uint32 IndirectArgOffset)
	{
		FComputeShaderUtils::ValidateIndirectArgsBuffer(IndirectArgsBuffer->GetSize(), IndirectArgOffset);

		Private::PrepareDispatch(RHICmdList, ComputeShader, ShaderBindings, PassParameters);
		RHICmdList.DispatchIndirectComputeShader(IndirectArgsBuffer, IndirectArgOffset);
		Private::AfterDispatch(RHICmdList, ComputeShader);
	}
}
