// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "GlobalShader.h"
#include "RenderGraphFwd.h"
#include "ShaderParameterStruct.h"

namespace NiagaraStateless
{
	static constexpr uint32 MaxGpuSpawnInfos = 4;

	BEGIN_SHADER_PARAMETER_STRUCT(FSpawnInfoShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER_SCALAR_ARRAY(uint32,	SpawnInfo_NumActive, [MaxGpuSpawnInfos])
		SHADER_PARAMETER_SCALAR_ARRAY(uint32,	SpawnInfo_ParticleOffset, [MaxGpuSpawnInfos])
		SHADER_PARAMETER_SCALAR_ARRAY(uint32,	SpawnInfo_UniqueOffset, [MaxGpuSpawnInfos])
		SHADER_PARAMETER_SCALAR_ARRAY(float,	SpawnInfo_Time, [MaxGpuSpawnInfos])
		SHADER_PARAMETER_SCALAR_ARRAY(float,	SpawnInfo_Rate, [MaxGpuSpawnInfos])
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FCommonShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(uint32,				Common_RandomSeed)
		SHADER_PARAMETER(float,					Common_SimulationTime)
		SHADER_PARAMETER(float,					Common_SimulationDeltaTime)
		SHADER_PARAMETER(float,					Common_SimulationInvDeltaTime)
		SHADER_PARAMETER(FVector2f,				Common_LifetimeScaleBias)

		SHADER_PARAMETER(uint32,				Common_OutputBufferStride)
		SHADER_PARAMETER(uint32,				Common_GPUCountBufferOffset)
		SHADER_PARAMETER_UAV(RWBuffer<float>,	Common_FloatOutputBuffer)
		//SHADER_PARAMETER_UAV(RWBuffer<half>,	Common_HalfOutputBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<int>,		Common_IntOutputBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<int>,		Common_GPUCountBuffer)
		SHADER_PARAMETER_SRV(Buffer<float>,		Common_StaticFloatBuffer)
		SHADER_PARAMETER_SRV(Buffer<uint32>,	Common_ParameterBuffer)

		SHADER_PARAMETER_STRUCT_INCLUDE(FSpawnInfoShaderParameters,	SpawnParameters)
	END_SHADER_PARAMETER_STRUCT()

	class FSimulationShader : public FGlobalShader
	{
	public:
		using FParameters = FCommonShaderParameters;
		static constexpr uint32 ThreadGroupSize = 64;

		FSimulationShader() {}
		FSimulationShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer) { }

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), ThreadGroupSize);
			OutEnvironment.SetDefine(TEXT("NIAGARA_MAX_GPU_SPAWN_INFOS"), NiagaraStateless::MaxGpuSpawnInfos);
		}
	};
}
