// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderParameterStruct.h: API to submit all shader parameters in single function call.
=============================================================================*/

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "RHI.h"
#include "RHIDefinitions.h"
#include "RenderGraphResources.h"
#include "Serialization/MemoryImage.h"
#include "Shader.h"

class FRHICommandList;
class FRHIComputeCommandList;
class FRHIComputeShader;
class FRHIGraphicsShader;
class FRHIResource;
class FShaderParameterMap;
class FShaderParametersMetadata;
struct FRHIUniformBufferLayout;

template <typename FParameterStruct>
void BindForLegacyShaderParameters(FShader* Shader, int32 PermutationId, const FShaderParameterMap& ParameterMap, bool bShouldBindEverything = false)
{
	Shader->Bindings.BindForLegacyShaderParameters(Shader, PermutationId, ParameterMap, *FParameterStruct::FTypeInfo::GetStructMetadata(), bShouldBindEverything);
}

/** Tag a shader class to use the structured shader parameters API.
 *
 * class FMyShaderClassCS : public FGlobalShader
 * {
 *		DECLARE_GLOBAL_SHADER(FMyShaderClassCS);
 *		SHADER_USE_PARAMETER_STRUCT(FMyShaderClassCS, FGlobalShader);
 *
 *		BEGIN_SHADER_PARAMETER_STRUCT(FParameters)
 *			SHADER_PARAMETER(FMatrix44f, ViewToClip)
 *			//...
 *		END_SHADER_PARAMETER_STRUCT()
 * };
 *
 * Notes: Long term, this macro will no longer be needed. Instead, parameter binding will become the default behavior for shader declarations.
 */

#define SHADER_USE_PARAMETER_STRUCT_INTERNAL(ShaderClass, ShaderParentClass, bShouldBindEverything) \
	ShaderClass(const ShaderMetaType::CompiledShaderInitializerType& Initializer) \
		: ShaderParentClass(Initializer) \
	{ \
		BindForLegacyShaderParameters<FParameters>(this, Initializer.PermutationId, Initializer.ParameterMap, bShouldBindEverything); \
	} \
	\
	ShaderClass() \
	{ } \

// TODO(RDG): would not even need ShaderParentClass anymore. And in fact should not so Bindings.Bind() is not being called twice.
#define SHADER_USE_PARAMETER_STRUCT(ShaderClass, ShaderParentClass) \
	SHADER_USE_PARAMETER_STRUCT_INTERNAL(ShaderClass, ShaderParentClass, true) \
	\
	static inline const FShaderParametersMetadata* GetRootParametersMetadata() { return FParameters::FTypeInfo::GetStructMetadata(); }

/** Use when sharing shader parameter binding with legacy parameters in the base class; i.e. FMaterialShader or FMeshMaterialShader.
 *  Note that this disables validation that the parameter struct contains all shader bindings.
 */
#define SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(ShaderClass, ShaderParentClass) \
	SHADER_USE_PARAMETER_STRUCT_INTERNAL(ShaderClass, ShaderParentClass, false)

#define SHADER_USE_ROOT_PARAMETER_STRUCT(ShaderClass, ShaderParentClass) \
	static inline const FShaderParametersMetadata* GetRootParametersMetadata() { return FParameters::FTypeInfo::GetStructMetadata(); } \
	\
	ShaderClass(const ShaderMetaType::CompiledShaderInitializerType& Initializer) \
		: ShaderParentClass(Initializer) \
	{ \
		this->Bindings.BindForRootShaderParameters(this, Initializer.PermutationId, Initializer.ParameterMap); \
	} \
	\
	ShaderClass() \
	{ } \


 /** Dereferences the RHI resource from a shader parameter struct. */
inline FRHIResource* GetShaderParameterResourceRHI(const void* Contents, uint16 MemberOffset, EUniformBufferBaseType MemberType)
{
	checkSlow(Contents);
	if (IsShaderParameterTypeIgnoredByRHI(MemberType))
	{
		return nullptr;
	}

	const uint8* MemberPtr = (const uint8*)Contents + MemberOffset;

	if (IsRDGResourceReferenceShaderParameterType(MemberType))
	{
		const FRDGResource* ResourcePtr = *reinterpret_cast<const FRDGResource* const*>(MemberPtr);
		return ResourcePtr ? ResourcePtr->GetRHI() : nullptr;
	}
	else
	{
		return *reinterpret_cast<FRHIResource* const*>(MemberPtr);
	}
}

/** Validates that all resource parameters of a uniform buffer are set. */
#if DO_CHECK
extern RENDERCORE_API void ValidateShaderParameterResourcesRHI(const void* Contents, const FRHIUniformBufferLayout& Layout);
#else
FORCEINLINE void ValidateShaderParameterResourcesRHI(const void* Contents, const FRHIUniformBufferLayout& Layout) {}
#endif


/** Raise fatal error when a required shader parameter has not been set. */
extern RENDERCORE_API void EmitNullShaderParameterFatalError(const TShaderRef<FShader>& Shader, const FShaderParametersMetadata* ParametersMetadata, uint16 MemberOffset);

/** Validates that all resource parameters of a shader are set. */
#if DO_CHECK
extern RENDERCORE_API void ValidateShaderParameters(const TShaderRef<FShader>& Shader, const FShaderParametersMetadata* ParametersMetadata, const void* Parameters);
#else
FORCEINLINE void ValidateShaderParameters(const TShaderRef<FShader>& Shader, const FShaderParametersMetadata* ParametersMetadata, const void* Parameters) {}
#endif

template<typename TShaderClass>
FORCEINLINE void ValidateShaderParameters(const TShaderRef<TShaderClass>& Shader, const typename TShaderClass::FParameters& Parameters)
{
	const typename TShaderClass::FParameters* ParameterPtr = &Parameters;
	return ValidateShaderParameters(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterPtr);
}

/** Unset compute shader UAVs. */
template<typename TRHICmdList, typename TShaderClass>
inline void UnsetShaderUAVs(TRHICmdList& RHICmdList, const TShaderRef<TShaderClass>& Shader, FRHIComputeShader* ShadeRHI)
{
	// TODO(RDG): Once all shader sets their parameter through this, can refactor RHI so all UAVs of a shader get unset through a single RHI function call.
	const FShaderParameterBindings& Bindings = Shader->Bindings;

	checkf(Bindings.RootParameterBufferIndex == FShaderParameterBindings::kInvalidBufferIndex, TEXT("Can't use UnsetShaderUAVs() for root parameter buffer index."));

	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.ResourceParameters)
	{
		if (ParameterBinding.BaseType == UBMT_UAV ||
			ParameterBinding.BaseType == UBMT_RDG_TEXTURE_UAV ||
			ParameterBinding.BaseType == UBMT_RDG_BUFFER_UAV)
		{
			RHICmdList.SetUAVParameter(ShadeRHI, ParameterBinding.BaseIndex, nullptr);
		}
	}
}

/** Unset compute shader SRVs. */
template<typename TRHICmdList, typename TShaderClass>
inline void UnsetShaderSRVs(TRHICmdList& RHICmdList, const TShaderRef<TShaderClass>& Shader, FRHIComputeShader* ShadeRHI)
{
	const FShaderParameterBindings& Bindings = Shader->Bindings;

	checkf(Bindings.RootParameterBufferIndex == FShaderParameterBindings::kInvalidBufferIndex, TEXT("Can't use UnsetShaderSRVs() for root parameter buffer index."));

	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.ResourceParameters)
	{
		if (ParameterBinding.BaseType == UBMT_SRV ||
			ParameterBinding.BaseType == UBMT_RDG_TEXTURE_SRV ||
			ParameterBinding.BaseType == UBMT_RDG_BUFFER_SRV)
		{
			RHICmdList.SetShaderResourceViewParameter(ShadeRHI, ParameterBinding.BaseIndex, nullptr);
		}
	}
}

RENDERCORE_API void SetShaderParameters(
	FRHIComputeCommandList& RHICmdList,
	FRHIComputeShader* ShaderRHI,
	const FShaderParameterBindings& Bindings,
	const FShaderParametersMetadata* ParametersMetadata,
	const uint8* Base);

RENDERCORE_API void SetShaderParameters(
	FRHICommandList& RHICmdList,
	FRHIGraphicsShader* ShaderRHI,
	const FShaderParameterBindings& Bindings,
	const FShaderParametersMetadata* ParametersMetadata,
	const uint8* Base);

RENDERCORE_API void SetShaderParameters(
	FRHICommandList& RHICmdList,
	FRHIComputeShader* ShaderRHI,
	const FShaderParameterBindings& Bindings,
	const FShaderParametersMetadata* ParametersMetadata,
	const uint8* Base);

template<typename TRHICmdList, typename TShaderClass, typename TShaderRHI>
inline void SetShaderParameters(
	TRHICmdList& RHICmdList,
	const TShaderRef<TShaderClass>& Shader,
	TShaderRHI* ShaderRHI,
	const FShaderParametersMetadata* ParametersMetadata,
	const typename TShaderClass::FParameters& Parameters)
{
	ValidateShaderParameters(Shader, ParametersMetadata, &Parameters);

	// TODO(RDG): Once all shader sets their parameter through this, can refactor RHI so all shader parameters get sets through a single RHI function call.
	const FShaderParameterBindings& Bindings = Shader->Bindings;

	const typename TShaderClass::FParameters* ParametersPtr = &Parameters;
	const uint8* Base = reinterpret_cast<const uint8*>(ParametersPtr);

	SetShaderParameters(RHICmdList, ShaderRHI, Bindings, ParametersMetadata, Base);
}

template<typename TRHICmdList, typename TShaderClass, typename TShaderRHI>
inline void SetShaderParameters(TRHICmdList& RHICmdList, const TShaderRef<TShaderClass>& Shader, TShaderRHI* ShaderRHI, const typename TShaderClass::FParameters& Parameters)
{
	const FShaderParametersMetadata* ParametersMetadata = TShaderClass::FParameters::FTypeInfo::GetStructMetadata();
	SetShaderParameters(RHICmdList, Shader, ShaderRHI, ParametersMetadata, Parameters);
}

#if RHI_RAYTRACING

RENDERCORE_API void SetShaderParameters(
	FRayTracingShaderBindingsWriter& RTBindingsWriter,
	const FShaderParameterBindings& Bindings,
	const FRHIUniformBufferLayout* RootUniformBufferLayout,
	const uint8* Base);

/** Set shader's parameters from its parameters struct. */
template<typename TShaderClass>
void SetShaderParameters(FRayTracingShaderBindingsWriter& RTBindingsWriter, const TShaderRef<TShaderClass>& Shader, const typename TShaderClass::FParameters& Parameters)
{
	ValidateShaderParameters(Shader, Parameters);

	const FShaderParameterBindings& Bindings = Shader->Bindings;

	checkf(Bindings.Parameters.Num() == 0, TEXT("Ray tracing shader should use SHADER_USE_ROOT_PARAMETER_STRUCT() to passdown the cbuffer layout to the shader compiler."));

	const typename TShaderClass::FParameters* ParametersPtr = &Parameters;
	const uint8* Base = reinterpret_cast<const uint8*>(ParametersPtr);

	SetShaderParameters(RTBindingsWriter, Bindings, TShaderClass::FParameters::FTypeInfo::GetStructMetadata()->GetLayoutPtr(), Base);
}

#endif // RHI_RAYTRACING
