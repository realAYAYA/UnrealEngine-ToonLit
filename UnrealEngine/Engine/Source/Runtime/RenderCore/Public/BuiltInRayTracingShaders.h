// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalShader.h"
#include "RHIDefinitions.h"
#include "ShaderParameterMacros.h"
#include "ShaderParameterStruct.h"

#if RHI_RAYTRACING

class FBuiltInRayTracingShader : public FGlobalShader
{
protected:
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{}

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// Built-in ray tracing shaders are always compiled for RHIs that support them, regardless of whether RT is enabled for the project.
		return RHISupportsRayTracingShaders(Parameters.Platform);
	}

	FBuiltInRayTracingShader() = default;
	FBuiltInRayTracingShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

class RENDERCORE_API UE_DEPRECATED(5.1, "Please use an explicit ray generation shader instead.") FOcclusionMainRG : public FBuiltInRayTracingShader
{
	DECLARE_GLOBAL_SHADER(FOcclusionMainRG);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FOcclusionMainRG, FBuiltInRayTracingShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FBasicRayData>, Rays)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OcclusionOutput)
	END_SHADER_PARAMETER_STRUCT()
};

class RENDERCORE_API UE_DEPRECATED(5.1, "Please use an explicit ray generation shader instead.") FIntersectionMainRG : public FBuiltInRayTracingShader
{
	DECLARE_GLOBAL_SHADER(FIntersectionMainRG);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FIntersectionMainRG, FBuiltInRayTracingShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FBasicRayData>, Rays)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FIntersectionPayload>, IntersectionOutput)
	END_SHADER_PARAMETER_STRUCT()
};

class UE_DEPRECATED(5.1, "Please use an explicit ray generation and hit shaders instead.") FIntersectionMainCHS : public FBuiltInRayTracingShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FIntersectionMainCHS, Global, RENDERCORE_API);
public:

	FIntersectionMainCHS() = default;
	FIntersectionMainCHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBuiltInRayTracingShader(Initializer)
	{}
};

class FDefaultMainCHS : public FBuiltInRayTracingShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FDefaultMainCHS, Global, RENDERCORE_API);
public:

	FDefaultMainCHS() = default;
	FDefaultMainCHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBuiltInRayTracingShader(Initializer)
	{}
};

class FDefaultMainCHSOpaqueAHS : public FBuiltInRayTracingShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FDefaultMainCHSOpaqueAHS, Global, RENDERCORE_API);
public:

	FDefaultMainCHSOpaqueAHS() = default;
	FDefaultMainCHSOpaqueAHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBuiltInRayTracingShader(Initializer)
	{}
};

class FDefaultPayloadMS : public FBuiltInRayTracingShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FDefaultPayloadMS, Global, RENDERCORE_API);
public:

	FDefaultPayloadMS() = default;
	FDefaultPayloadMS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBuiltInRayTracingShader(Initializer)
	{}
};

class FPackedMaterialClosestHitPayloadMS : public FBuiltInRayTracingShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FPackedMaterialClosestHitPayloadMS, Global, RENDERCORE_API);
public:

	FPackedMaterialClosestHitPayloadMS() = default;
	FPackedMaterialClosestHitPayloadMS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBuiltInRayTracingShader(Initializer)
	{}
};

class RENDERCORE_API FRayTracingDispatchDescCS : public FBuiltInRayTracingShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingDispatchDescCS);

public:
	FRayTracingDispatchDescCS() = default;
	FRayTracingDispatchDescCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBuiltInRayTracingShader(Initializer)
	{
		DispatchDescInputParam.Bind(Initializer.ParameterMap, TEXT("DispatchDescInput"), SPF_Mandatory);
		DispatchDescSizeDwordsParam.Bind(Initializer.ParameterMap, TEXT("DispatchDescSizeDwords"), SPF_Mandatory);
		DispatchDescDimensionsOffsetDwordsParam.Bind(Initializer.ParameterMap, TEXT("DispatchDescDimensionsOffsetDwords"), SPF_Mandatory);
		DimensionsBufferOffsetDwordsParam.Bind(Initializer.ParameterMap, TEXT("DimensionsBufferOffsetDwords"), SPF_Mandatory);
		DispatchDimensionsParam.Bind(Initializer.ParameterMap, TEXT("DispatchDimensions"), SPF_Mandatory);
		DispatchDescOutputParam.Bind(Initializer.ParameterMap, TEXT("DispatchDescOutput"), SPF_Mandatory);
	}

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DISPATCH_DESC_MAX_SIZE_DWORDS"), DispatchDescMaxSizeDwords);
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static void Dispatch(FRHICommandList& RHICmdList,
		const void* DispatchDescInput, uint32 DispatchDescSize, uint32 DispatchDescDimensionsOffset,
		FRHIShaderResourceView* DispatchDimensionsSRV, uint32 DimensionsBufferOffset,
		FRHIUnorderedAccessView* DispatchDescOutputUAV);

	static constexpr uint32 DispatchDescMaxSizeDwords = 32;

	LAYOUT_FIELD(FShaderParameter, DispatchDescInputParam);
	LAYOUT_FIELD(FShaderParameter, DispatchDescSizeDwordsParam);
	LAYOUT_FIELD(FShaderParameter, DispatchDescDimensionsOffsetDwordsParam);
	LAYOUT_FIELD(FShaderParameter, DimensionsBufferOffsetDwordsParam);
	LAYOUT_FIELD(FShaderResourceParameter, DispatchDimensionsParam);
	LAYOUT_FIELD(FShaderResourceParameter, DispatchDescOutputParam);
};

#endif // RHI_RAYTRACING

