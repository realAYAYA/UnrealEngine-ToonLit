// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"

enum class EUpdateTextureValueType
{
	Float,
	Int32,
	Uint32
};

class FUpdateTexture2DSubresourceCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FUpdateTexture2DSubresourceCS, Global, ENGINE_API);
public:
	FUpdateTexture2DSubresourceCS() {}
	FUpdateTexture2DSubresourceCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader( Initializer )
	{
		SrcPosPitchParameter.Bind(Initializer.ParameterMap, TEXT("TSrcPosPitch"), SPF_Mandatory);
		SrcBuffer.Bind(Initializer.ParameterMap, TEXT("TSrcBuffer"), SPF_Mandatory);
		DestPosSizeParameter.Bind(Initializer.ParameterMap, TEXT("TDestPosSize"), SPF_Mandatory);
		DestTexture.Bind(Initializer.ParameterMap, TEXT("TDestTexture"), SPF_Mandatory);
	}

	LAYOUT_FIELD(FShaderParameter, SrcPosPitchParameter);
	LAYOUT_FIELD(FShaderResourceParameter, SrcBuffer);
	LAYOUT_FIELD(FShaderParameter, DestPosSizeParameter);
	LAYOUT_FIELD(FShaderResourceParameter, DestTexture);

	static inline TShaderRef<FUpdateTexture2DSubresourceCS> SelectShader(FGlobalShaderMap* GlobalShaderMap, EUpdateTextureValueType ValueType);

protected:
	template<typename ShaderType>
	static inline TShaderRef<FUpdateTexture2DSubresourceCS> SelectShader(FGlobalShaderMap* GlobalShaderMap);
};

template<EUpdateTextureValueType ValueType>
class TUpdateTexture2DSubresourceCS : public FUpdateTexture2DSubresourceCS
{
	DECLARE_EXPORTED_SHADER_TYPE(TUpdateTexture2DSubresourceCS, Global, ENGINE_API);
public:
	TUpdateTexture2DSubresourceCS() {}
	TUpdateTexture2DSubresourceCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FUpdateTexture2DSubresourceCS(Initializer)
	{}

	static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/UpdateTextureShaders.usf"); }
	
	static const TCHAR* GetFunctionName() { return TEXT("TUpdateTexture2DSubresourceCS"); }

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		const TCHAR* Type = nullptr;
		switch (ValueType)
		{
		default: checkNoEntry();
		case EUpdateTextureValueType::Float: OutEnvironment.SetDefine(TEXT("COMPONENT_TYPE"), TEXT("float4")); break;
		case EUpdateTextureValueType::Int32: OutEnvironment.SetDefine(TEXT("COMPONENT_TYPE"), TEXT("int4")); break;
		case EUpdateTextureValueType::Uint32: OutEnvironment.SetDefine(TEXT("COMPONENT_TYPE"), TEXT("uint4")); break;
		}
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

template <typename ShaderType>
inline TShaderRef<FUpdateTexture2DSubresourceCS> FUpdateTexture2DSubresourceCS::SelectShader(FGlobalShaderMap* GlobalShaderMap)
{
	TShaderRef<ShaderType> Shader = GlobalShaderMap->GetShader<ShaderType>();
	return Shader;
}

inline TShaderRef<FUpdateTexture2DSubresourceCS> FUpdateTexture2DSubresourceCS::SelectShader(FGlobalShaderMap* GlobalShaderMap, EUpdateTextureValueType ValueType)
{
	switch (ValueType)
	{
	default: checkNoEntry();
	case EUpdateTextureValueType::Float: return FUpdateTexture2DSubresourceCS::SelectShader<TUpdateTexture2DSubresourceCS<EUpdateTextureValueType::Float>>(GlobalShaderMap);
	case EUpdateTextureValueType::Int32: return FUpdateTexture2DSubresourceCS::SelectShader<TUpdateTexture2DSubresourceCS<EUpdateTextureValueType::Int32>>(GlobalShaderMap);
	case EUpdateTextureValueType::Uint32: return FUpdateTexture2DSubresourceCS::SelectShader<TUpdateTexture2DSubresourceCS<EUpdateTextureValueType::Uint32>>(GlobalShaderMap);
	}
}

class FUpdateTexture3DSubresourceCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FUpdateTexture3DSubresourceCS, Global, ENGINE_API);
public:
	FUpdateTexture3DSubresourceCS() {}
	FUpdateTexture3DSubresourceCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SrcPitchParameter.Bind(Initializer.ParameterMap, TEXT("SrcPitch"), SPF_Mandatory);
		SrcDepthPitchParameter.Bind(Initializer.ParameterMap, TEXT("SrcDepthPitch"), SPF_Mandatory);

		SrcBuffer.Bind(Initializer.ParameterMap, TEXT("SrcBuffer"), SPF_Mandatory);

		DestPosParameter.Bind(Initializer.ParameterMap, TEXT("DestPos"), SPF_Mandatory);
		DestSizeParameter.Bind(Initializer.ParameterMap, TEXT("DestSize"), SPF_Mandatory);

		DestTexture3D.Bind(Initializer.ParameterMap, TEXT("DestTexture3D"), SPF_Mandatory);
	}

	static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/UpdateTextureShaders.usf"); }
	
	static const TCHAR* GetFunctionName() { return TEXT("UpdateTexture3DSubresourceCS"); };

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	LAYOUT_FIELD(FShaderParameter, SrcPitchParameter);
	LAYOUT_FIELD(FShaderParameter, SrcDepthPitchParameter);
	LAYOUT_FIELD(FShaderResourceParameter, SrcBuffer);

	LAYOUT_FIELD(FShaderParameter, DestPosParameter);
	LAYOUT_FIELD(FShaderParameter, DestSizeParameter);

	LAYOUT_FIELD(FShaderResourceParameter, DestTexture3D)
};

template<uint32 ElementsPerThread>
class TCopyDataCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(TCopyDataCS, Global, ENGINE_API);
public:
	TCopyDataCS() {}
	TCopyDataCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SrcBuffer.Bind(Initializer.ParameterMap, TEXT("SrcCopyBuffer"), SPF_Mandatory);
		DestBuffer.Bind(Initializer.ParameterMap, TEXT("DestBuffer"), SPF_Mandatory);		
	}

	static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/UpdateTextureShaders.usf"); }

	static const TCHAR* GetFunctionName() 
	{
		switch (ElementsPerThread)
		{
			case 1u: return TEXT("CopyData1CS"); break;
			case 2u: return TEXT("CopyData2CS"); break;
		}
		return nullptr;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	LAYOUT_FIELD(FShaderResourceParameter, SrcBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, DestBuffer);
};
