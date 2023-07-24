// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "HAL/Platform.h"
#include "RHIDefinitions.h"
#include "Serialization/MemoryLayout.h"
#include "Shader.h"
#include "ShaderCore.h"
#include "ShaderParameters.h"

class FPointerTableBase;
class FRHICommandList;
struct FResolveRect;

struct FDummyResolveParameter {};

class RENDERCORE_API FResolveDepthPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FResolveDepthPS);
public:
	typedef FDummyResolveParameter FParameter;
		
	FResolveDepthPS();
	FResolveDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	void SetParameters(FRHICommandList& RHICmdList, FParameter);

	LAYOUT_FIELD(FShaderResourceParameter, UnresolvedSurface);
};

class RENDERCORE_API FResolveDepth2XPS : public FResolveDepthPS
{
	DECLARE_GLOBAL_SHADER(FResolveDepth2XPS);
public:
	typedef FDummyResolveParameter FParameter;

	FResolveDepth2XPS();
	FResolveDepth2XPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

class RENDERCORE_API FResolveDepth4XPS : public FResolveDepthPS
{
	DECLARE_GLOBAL_SHADER(FResolveDepth4XPS);
public:
	typedef FDummyResolveParameter FParameter;

	FResolveDepth4XPS();
	FResolveDepth4XPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

class RENDERCORE_API FResolveDepth8XPS : public FResolveDepthPS
{
	DECLARE_GLOBAL_SHADER(FResolveDepth8XPS);
public:
	typedef FDummyResolveParameter FParameter;

	FResolveDepth8XPS();
	FResolveDepth8XPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	static bool ShouldCache(EShaderPlatform Platform);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

class RENDERCORE_API FResolveDepthArrayPS : public FResolveDepthPS
{
	DECLARE_GLOBAL_SHADER(FResolveDepthArrayPS);
public:
	typedef FDummyResolveParameter FParameter;

	FResolveDepthArrayPS();
	FResolveDepthArrayPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

class RENDERCORE_API FResolveDepthArray2XPS : public FResolveDepthArrayPS
{
	DECLARE_GLOBAL_SHADER(FResolveDepthArray2XPS);
public:
	typedef FDummyResolveParameter FParameter;

	FResolveDepthArray2XPS();
	FResolveDepthArray2XPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

class RENDERCORE_API FResolveDepthArray4XPS : public FResolveDepthArrayPS
{
	DECLARE_GLOBAL_SHADER(FResolveDepthArray4XPS);
public:
	typedef FDummyResolveParameter FParameter;

	FResolveDepthArray4XPS();
	FResolveDepthArray4XPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

class RENDERCORE_API FResolveDepthArray8XPS : public FResolveDepthArrayPS
{
	DECLARE_GLOBAL_SHADER(FResolveDepthArray8XPS);
public:
	typedef FDummyResolveParameter FParameter;

	FResolveDepthArray8XPS();
	FResolveDepthArray8XPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	static bool ShouldCache(EShaderPlatform Platform);
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

class RENDERCORE_API FResolveSingleSamplePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FResolveSingleSamplePS);
public:
	typedef uint32 FParameter;

	FResolveSingleSamplePS();
	FResolveSingleSamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
	
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	void SetParameters(FRHICommandList& RHICmdList, uint32 SingleSampleIndexValue);
	
	LAYOUT_FIELD(FShaderResourceParameter, UnresolvedSurface);
	LAYOUT_FIELD(FShaderParameter, SingleSampleIndex);
};

/**
 * A vertex shader for rendering a textured screen element.
 */
class RENDERCORE_API FResolveVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FResolveVS);
public:
	FResolveVS();
	FResolveVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
	
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	void SetParameters(FRHICommandList& RHICmdList, const FResolveRect& SrcBounds, const FResolveRect& DstBounds, uint32 DstSurfaceWidth, uint32 DstSurfaceHeight);

	LAYOUT_FIELD(FShaderParameter, PositionMinMax);
	LAYOUT_FIELD(FShaderParameter, UVMinMax);
};

class RENDERCORE_API FResolveArrayVS : public FResolveVS
{
	DECLARE_GLOBAL_SHADER(FResolveArrayVS);
public:
	FResolveArrayVS();
	FResolveArrayVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	void SetParameters(FRHICommandList& RHICmdList, const FResolveRect& SrcBounds, const FResolveRect& DstBounds, uint32 DstSurfaceWidth, uint32 DstSurfaceHeight);
};
