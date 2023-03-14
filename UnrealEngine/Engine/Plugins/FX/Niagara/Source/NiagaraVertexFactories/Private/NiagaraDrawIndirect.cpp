// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraDrawIndirect.cpp : Niagara shader to generate the draw indirect args for Niagara renderers.
==============================================================================*/

#include "NiagaraDrawIndirect.h"
#include "NiagaraGPUSortInfo.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_GLOBAL_SHADER(FNiagaraDrawIndirectArgsGenCS, "/Plugin/FX/Niagara/Private/NiagaraDrawIndirectArgsGen.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FNiagaraDrawIndirectResetCountsCS, "/Plugin/FX/Niagara/Private/NiagaraDrawIndirectArgsGen.usf", "ResetCountsCS", SF_Compute);

void FNiagaraDrawIndirectArgsGenCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_COUNT"), NIAGARA_DRAW_INDIRECT_ARGS_GEN_THREAD_COUNT);
	OutEnvironment.SetDefine(TEXT("NIAGARA_DRAW_INDIRECT_ARGS_SIZE"), NIAGARA_DRAW_INDIRECT_ARGS_SIZE);
	OutEnvironment.SetDefine(TEXT("NIAGARA_DRAW_INDIRECT_TASK_INFO_SIZE"), NIAGARA_DRAW_INDIRECT_TASK_INFO_SIZE);
}

FNiagaraDrawIndirectArgsGenCS::FNiagaraDrawIndirectArgsGenCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
{
	TaskInfosParam.Bind(Initializer.ParameterMap, TEXT("TaskInfos"));
	CulledInstanceCountsParam.Bind(Initializer.ParameterMap, TEXT("CulledInstanceCounts"));
	InstanceCountsParam.Bind(Initializer.ParameterMap, TEXT("InstanceCounts"));
	DrawIndirectArgsParam.Bind(Initializer.ParameterMap, TEXT("DrawIndirectArgs"));
	TaskCountParam.Bind(Initializer.ParameterMap, TEXT("TaskCount"));
}

/*bool FNiagaraDrawIndirectArgsGenCS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << TaskInfosParam;
	Ar << CulledInstanceCountsParam;
	Ar << InstanceCountsParam;
	Ar << DrawIndirectArgsParam;
	Ar << TaskCountParam;
	return bShaderHasOutdatedParameters;
}*/

void FNiagaraDrawIndirectArgsGenCS::SetOutput(FRHICommandList& RHICmdList, FRHIUnorderedAccessView* DrawIndirectArgsUAV, FRHIUnorderedAccessView* InstanceCountsUAV)
{
	FRHIComputeShader* ComputeShaderRHI = RHICmdList.GetBoundComputeShader();
	if (DrawIndirectArgsParam.IsBound())
	{
		RHICmdList.SetUAVParameter(ComputeShaderRHI, DrawIndirectArgsParam.GetUAVIndex(), DrawIndirectArgsUAV);
	}
	if (InstanceCountsParam.IsBound())
	{
		RHICmdList.SetUAVParameter(ComputeShaderRHI, InstanceCountsParam.GetUAVIndex(), InstanceCountsUAV);
	}
}

void FNiagaraDrawIndirectArgsGenCS::SetParameters(FRHICommandList& RHICmdList, FRHIShaderResourceView* TaskInfosBuffer, FRHIShaderResourceView* CulledInstanceCountsBuffer, int32 ArgGenTaskOffset, int32 NumArgGenTasks, int32 NumInstanceCountClearTasks)
{
	FRHIComputeShader* ComputeShaderRHI = RHICmdList.GetBoundComputeShader();

	SetSRVParameter(RHICmdList, ComputeShaderRHI, TaskInfosParam, TaskInfosBuffer);
	SetSRVParameter(RHICmdList, ComputeShaderRHI, CulledInstanceCountsParam, CulledInstanceCountsBuffer);

	const FUintVector4 TaskCountValue(ArgGenTaskOffset, NumArgGenTasks, NumInstanceCountClearTasks, NumArgGenTasks + NumInstanceCountClearTasks);
	SetShaderValue(RHICmdList, ComputeShaderRHI, TaskCountParam, TaskCountValue);
}

void FNiagaraDrawIndirectArgsGenCS::UnbindBuffers(FRHICommandList& RHICmdList)
{
	FRHIComputeShader* ComputeShaderRHI = RHICmdList.GetBoundComputeShader();

	SetSRVParameter(RHICmdList, ComputeShaderRHI, TaskInfosParam, nullptr);
	if (DrawIndirectArgsParam.IsBound())
	{
		RHICmdList.SetUAVParameter(ComputeShaderRHI, DrawIndirectArgsParam.GetUAVIndex(), nullptr);
	}
	if (InstanceCountsParam.IsBound())
	{
		RHICmdList.SetUAVParameter(ComputeShaderRHI, InstanceCountsParam.GetUAVIndex(), nullptr);
	}
}
void FNiagaraDrawIndirectResetCountsCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_COUNT"), NIAGARA_DRAW_INDIRECT_ARGS_GEN_THREAD_COUNT);
	OutEnvironment.SetDefine(TEXT("NIAGARA_DRAW_INDIRECT_ARGS_SIZE"), NIAGARA_DRAW_INDIRECT_ARGS_SIZE);
	OutEnvironment.SetDefine(TEXT("NIAGARA_DRAW_INDIRECT_TASK_INFO_SIZE"), NIAGARA_DRAW_INDIRECT_TASK_INFO_SIZE);
}

FNiagaraDrawIndirectResetCountsCS::FNiagaraDrawIndirectResetCountsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
{
	TaskInfosParam.Bind(Initializer.ParameterMap, TEXT("TaskInfos"));
	InstanceCountsParam.Bind(Initializer.ParameterMap, TEXT("InstanceCounts"));
	TaskCountParam.Bind(Initializer.ParameterMap, TEXT("TaskCount"));
}

void FNiagaraDrawIndirectResetCountsCS::SetOutput(FRHICommandList& RHICmdList, FRHIUnorderedAccessView* InstanceCountsUAV)
{
	FRHIComputeShader* ComputeShaderRHI = RHICmdList.GetBoundComputeShader();

	if (InstanceCountsParam.IsBound())
	{
		RHICmdList.SetUAVParameter(ComputeShaderRHI, InstanceCountsParam.GetUAVIndex(), InstanceCountsUAV);
	}
}

void FNiagaraDrawIndirectResetCountsCS::SetParameters(FRHICommandList& RHICmdList, FRHIShaderResourceView* TaskInfosBuffer, int32 NumArgGenTasks, int32 NumInstanceCountClearTasks)
{
	FRHIComputeShader* ComputeShaderRHI = RHICmdList.GetBoundComputeShader();

	SetSRVParameter(RHICmdList, ComputeShaderRHI, TaskInfosParam, TaskInfosBuffer);

	const FUintVector4 TaskCountValue(0, NumArgGenTasks, NumInstanceCountClearTasks, NumArgGenTasks + NumInstanceCountClearTasks);
	SetShaderValue(RHICmdList, ComputeShaderRHI, TaskCountParam, TaskCountValue);
}

void FNiagaraDrawIndirectResetCountsCS::UnbindBuffers(FRHICommandList& RHICmdList)
{
	FRHIComputeShader* ComputeShaderRHI = RHICmdList.GetBoundComputeShader();

	SetSRVParameter(RHICmdList, ComputeShaderRHI, TaskInfosParam, nullptr);

	if (InstanceCountsParam.IsBound())
	{
		RHICmdList.SetUAVParameter(ComputeShaderRHI, InstanceCountsParam.GetUAVIndex(), nullptr);
	}
}