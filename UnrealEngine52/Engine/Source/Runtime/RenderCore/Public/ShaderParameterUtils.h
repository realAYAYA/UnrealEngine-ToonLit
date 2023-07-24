// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderParameters.h: Shader parameter inline definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "ShaderParameters.h"
#include "ShaderCore.h"
#include "Misc/App.h"

template<typename TBufferStruct> class TUniformBuffer;
template<typename TBufferStruct> class TUniformBufferRef;

/**
 * Sets the value of a  shader parameter.  Template'd on shader type
 * A template parameter specified the type of the parameter value.
 * NOTE: Shader should be the param ref type, NOT the param type, since Shader is passed by value. 
 * Otherwise AddRef/ReleaseRef will be called many times.
 */
template<typename ShaderRHIParamRef, class ParameterType, typename TRHICmdList>
void SetShaderValue(
	TRHICmdList& RHICmdList,
	const ShaderRHIParamRef& Shader,
	const FShaderParameter& Parameter,
	const ParameterType& Value,
	uint32 ElementIndex = 0
	)
{
	static_assert(!TIsPointer<ParameterType>::Value, "Passing by value is not valid.");

	const uint32 AlignedTypeSize = (uint32)Align(sizeof(ParameterType), SHADER_PARAMETER_ARRAY_ELEMENT_ALIGNMENT);
	const int32 NumBytesToSet = FMath::Min<int32>(sizeof(ParameterType),Parameter.GetNumBytes() - ElementIndex * AlignedTypeSize);

	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if(NumBytesToSet > 0)
	{
		RHICmdList.SetShaderParameter(
			Shader,
			Parameter.GetBufferIndex(),
			Parameter.GetBaseIndex() + ElementIndex * AlignedTypeSize,
			(uint32)NumBytesToSet,
			&Value
			);
	}
}


template<typename ShaderRHIParamRef, class ParameterType>
void SetShaderValueOnContext(
	IRHICommandContext& RHICmdListContext,
	const ShaderRHIParamRef& Shader,
	const FShaderParameter& Parameter,
	const ParameterType& Value,
	uint32 ElementIndex = 0
	)
{
	static_assert(!TIsPointer<ParameterType>::Value, "Passing by value is not valid.");

	const uint32 AlignedTypeSize = Align(sizeof(ParameterType), SHADER_PARAMETER_ARRAY_ELEMENT_ALIGNMENT);
	const int32 NumBytesToSet = FMath::Min<int32>(sizeof(ParameterType), Parameter.GetNumBytes() - ElementIndex * AlignedTypeSize);

	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if (NumBytesToSet > 0)
	{
		RHICmdListContext.RHISetShaderParameter(
			Shader,
			Parameter.GetBufferIndex(),
			Parameter.GetBaseIndex() + ElementIndex * AlignedTypeSize,
			(uint32)NumBytesToSet,
			&Value
			);
	}
}

/**
 * Sets the value of a shader parameter array.  Template'd on shader type
 * A template parameter specified the type of the parameter value.
 * NOTE: Shader should be the param ref type, NOT the param type, since Shader is passed by value. 
 * Otherwise AddRef/ReleaseRef will be called many times.
 */
template<typename ShaderRHIParamRef,class ParameterType, typename TRHICmdList>
void SetShaderValueArray(
	TRHICmdList& RHICmdList,
	const ShaderRHIParamRef& Shader,
	const FShaderParameter& Parameter,
	const ParameterType* Values,
	uint32 NumElements,
	uint32 BaseElementIndex = 0
	)
{
	const uint32 AlignedTypeSize = (uint32)Align(sizeof(ParameterType), SHADER_PARAMETER_ARRAY_ELEMENT_ALIGNMENT);
	const int32 NumBytesToSet = FMath::Min<int32>(NumElements * AlignedTypeSize,Parameter.GetNumBytes() - BaseElementIndex * AlignedTypeSize);

	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if(NumBytesToSet > 0)
	{
		RHICmdList.SetShaderParameter(
			Shader,
			Parameter.GetBufferIndex(),
			Parameter.GetBaseIndex() + BaseElementIndex * AlignedTypeSize,
			(uint32)NumBytesToSet,
			Values
			);
	}
}

/** Specialization of the above for C++ bool type. */
template<typename ShaderRHIParamRef, typename TRHICmdList>
void SetShaderValueArray(
	TRHICmdList& RHICmdList,
	const ShaderRHIParamRef& Shader,
	const FShaderParameter& Parameter,
	const bool* Values,
	uint32 NumElements,
	uint32 BaseElementIndex = 0
	)
{
	UE_LOG(LogShaders, Fatal, TEXT("SetShaderValueArray does not support bool arrays."));
}


// LWC_TODO: Setting guards to catch attempts to pass a type with double components. Could just convert these to the correct type internally, but would prefer to catch potential issues + optimize where possible.
#define GUARD_SETSHADERVALUE(_TYPE)																																						\
template<typename ShaderRHIParamRef, typename TRHICmdList>																																\
void SetShaderValue(  TRHICmdList& RHICmdList,	const ShaderRHIParamRef& Shader, const FShaderParameter& Parameter,																		\
	const _TYPE##d& Value, uint32 ElementIndex = 0) { static_assert(sizeof(ShaderRHIParamRef) == 0, "Passing unsupported "#_TYPE"d. Requires "#_TYPE"f"); }								\
template<typename ShaderRHIParamRef>																																					\
void SetShaderValueOnContext(IRHICommandContext& RHICmdListContext,	const ShaderRHIParamRef& Shader, const FShaderParameter& Parameter,													\
	const _TYPE##d& Value, uint32 ElementIndex = 0) { static_assert(sizeof(ShaderRHIParamRef) == 0, "Passing unsupported "#_TYPE"d. Requires "#_TYPE"f"); }								\
template<typename ShaderRHIParamRef, typename TRHICmdList>																																\
void SetShaderValueArray(TRHICmdList& RHICmdList, const ShaderRHIParamRef& Shader, const FShaderParameter& Parameter,																	\
	const _TYPE##d* Values, uint32 NumElements, uint32 BaseElementIndex = 0) { static_assert(sizeof(ShaderRHIParamRef) == 0, "Passing unsupported "#_TYPE"d*. Requires "#_TYPE"f*"); }	\

// Primary
GUARD_SETSHADERVALUE(FMatrix44)
GUARD_SETSHADERVALUE(FVector2)
GUARD_SETSHADERVALUE(FVector3)
GUARD_SETSHADERVALUE(FVector4)
GUARD_SETSHADERVALUE(FPlane4)
GUARD_SETSHADERVALUE(FQuat4)
// Secondary
GUARD_SETSHADERVALUE(FSphere3)
GUARD_SETSHADERVALUE(FBox3)

/**
 * Sets the value of a shader surface parameter (e.g. to access MSAA samples).
 * Template'd on shader type (e.g. pixel shader or compute shader).
 */
template<typename TRHIShader, typename TRHICmdList>
UE_DEPRECATED(5.2, "SetTextureParameter with an index can't be supported anymore. Your code should be changed to use shader parameter structs to utilize resource arrays.")
FORCEINLINE void SetTextureParameter(TRHICmdList& RHICmdList, TRHIShader* Shader, const FShaderResourceParameter& Parameter, FRHITexture* TextureRHI, uint32 ElementIndex)
{
	if (Parameter.IsBound() && ElementIndex < Parameter.GetNumResources())
	{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
		if (Parameter.GetType() == EShaderParameterType::BindlessResourceIndex)
		{
			checkf(ElementIndex == 0, TEXT("Bindless resources don't support element offsets"));
			RHICmdList.SetBindlessTexture(Shader, Parameter.GetBaseIndex(), TextureRHI);
		}
		else
#endif
		{
			RHICmdList.SetShaderTexture(Shader, Parameter.GetBaseIndex() + ElementIndex, TextureRHI);
		}
	}
}

template<typename TRHIShader, typename TRHICmdList>
FORCEINLINE void SetTextureParameter(TRHICmdList& RHICmdList, TRHIShader* Shader, const FShaderResourceParameter& Parameter, FRHITexture* TextureRHI)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SetTextureParameter(RHICmdList, Shader, Parameter, TextureRHI, 0);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

/**
 * Sets the value of a shader sampler parameter. Template'd on shader type.
 */
template<typename TRHIShader, typename TRHICmdList>
UE_DEPRECATED(5.2, "SetSamplerParameter with an index can't be supported anymore. Your code should be changed to use shader parameter structs to utilize resource arrays.")
FORCEINLINE void SetSamplerParameter(TRHICmdList& RHICmdList, TRHIShader* Shader, const FShaderResourceParameter& Parameter, FRHISamplerState* SamplerStateRHI, uint32 ElementIndex)
{
	if (Parameter.IsBound() && ElementIndex < Parameter.GetNumResources())
	{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
		if (Parameter.GetType() == EShaderParameterType::BindlessSamplerIndex)
		{
			checkf(ElementIndex == 0, TEXT("Bindless resources don't support element offsets"));
			RHICmdList.SetBindlessSampler(Shader, Parameter.GetBaseIndex(), SamplerStateRHI);
		}
		else
#endif
		{
			RHICmdList.SetShaderSampler(Shader, Parameter.GetBaseIndex() + ElementIndex, SamplerStateRHI);
		}
	}
}

template<typename TRHIShader, typename TRHICmdList>
FORCEINLINE void SetSamplerParameter(TRHICmdList& RHICmdList, TRHIShader* Shader, const FShaderResourceParameter& Parameter, FRHISamplerState* SamplerStateRHI)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SetSamplerParameter(RHICmdList, Shader, Parameter, SamplerStateRHI, 0);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

/**
 * Sets the value of a shader texture parameter. Template'd on shader type.
 */
template<typename TRHIShader, typename TRHICmdList>
UE_DEPRECATED(5.2, "SetTextureParameter with an index can't be supported anymore. Your code should be changed to use shader parameter structs to utilize resource arrays.")
FORCEINLINE void SetTextureParameter(
	TRHICmdList& RHICmdList,
	TRHIShader* Shader,
	const FShaderResourceParameter& TextureParameter,
	const FShaderResourceParameter& SamplerParameter,
	FRHISamplerState* SamplerStateRHI,
	FRHITexture* TextureRHI,
	uint32 ElementIndex
	)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SetTextureParameter(RHICmdList, Shader, TextureParameter, TextureRHI, ElementIndex);
	
	// @todo UE samplerstate Should we maybe pass in two separate values? SamplerElement and TextureElement? Or never allow an array of samplers? Unsure best
	// if there is a matching sampler for this texture array index (ElementIndex), then set it. This will help with this case:
	//			Texture2D LightMapTextures[NUM_LIGHTMAP_COEFFICIENTS];
	//			SamplerState LightMapTexturesSampler;
	// In this case, we only set LightMapTexturesSampler when ElementIndex is 0, we don't set the sampler state for all 4 textures
	// This assumes that the all textures want to use the same sampler state

	SetSamplerParameter(RHICmdList, Shader, SamplerParameter, SamplerStateRHI, ElementIndex);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

template<typename TRHIShader, typename TRHICmdList>
FORCEINLINE void SetTextureParameter(TRHICmdList& RHICmdList, TRHIShader* Shader, const FShaderResourceParameter& TextureParameter, const FShaderResourceParameter& SamplerParameter, FRHISamplerState* SamplerStateRHI, FRHITexture* TextureRHI)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SetTextureParameter(RHICmdList, Shader, TextureParameter, SamplerParameter, SamplerStateRHI, TextureRHI, 0);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

/**
 * Sets the value of a shader texture parameter.  Template'd on shader type
 */
template<typename TRHIShader, typename TRHICmdList>
UE_DEPRECATED(5.2, "SetTextureParameter with an index can't be supported anymore. Your code should be changed to use shader parameter structs to utilize resource arrays.")
FORCEINLINE void SetTextureParameter(
	TRHICmdList& RHICmdList,
	TRHIShader* Shader,
	const FShaderResourceParameter& TextureParameter,
	const FShaderResourceParameter& SamplerParameter,
	const FTexture* Texture,
	uint32 ElementIndex
)
{
	if (TextureParameter.IsBound())
	{
		Texture->LastRenderTime = FApp::GetCurrentTime();
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SetTextureParameter(RHICmdList, Shader, TextureParameter, Texture->TextureRHI, ElementIndex);
	SetSamplerParameter(RHICmdList, Shader, SamplerParameter, Texture->SamplerStateRHI, ElementIndex);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

template<typename TRHIShader, typename TRHICmdList>
FORCEINLINE void SetTextureParameter(TRHICmdList& RHICmdList, TRHIShader* Shader, const FShaderResourceParameter& TextureParameter, const FShaderResourceParameter& SamplerParameter, const FTexture* Texture)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SetTextureParameter(RHICmdList, Shader, TextureParameter, SamplerParameter, Texture, 0);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

/**
 * Sets the value of a shader resource view parameter
 * Template'd on shader type (e.g. pixel shader or compute shader).
 */
template<typename TRHIShader, typename TRHICmdList>
FORCEINLINE void SetSRVParameter(TRHICmdList& RHICmdList, TRHIShader* Shader, const FShaderResourceParameter& Parameter, FRHIShaderResourceView* SRV)
{
	if (Parameter.IsBound())
	{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
		if (Parameter.GetType() == EShaderParameterType::BindlessResourceIndex)
		{
			RHICmdList.SetBindlessResourceView(Shader, Parameter.GetBaseIndex(), SRV);
		}
		else
#endif
		{
			RHICmdList.SetShaderResourceViewParameter(Shader, Parameter.GetBaseIndex(), SRV);
		}
	}
}

template<typename TRHIShader, typename TRHICmdList>
FORCEINLINE void SetSRVParameter(TRHICmdList& RHICmdList, const TRefCountPtr<TRHIShader>& Shader, const FShaderResourceParameter& Parameter, FRHIShaderResourceView* SRV)
{
	SetSRVParameter(RHICmdList, Shader.GetReference(), Parameter, SRV);
}

template<typename TRHIShader, typename TRHICmdList>
FORCEINLINE void SetUAVParameterSafeShader(TRHICmdList& RHICmdList, TRHIShader* Shader, const FShaderResourceParameter& Parameter, FRHIUnorderedAccessView* UAV)
{
	if (Parameter.IsBound())
	{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
		if (Parameter.GetType() == EShaderParameterType::BindlessResourceIndex)
		{
			RHICmdList.SetBindlessUAV(Shader, Parameter.GetBaseIndex(), UAV);
		}
		else
#endif
		{
			RHICmdList.SetUAVParameter(Shader, Parameter.GetBaseIndex(), UAV);
		}
	}
}

FORCEINLINE void SetUAVParameter(FRHIComputeCommandList& RHICmdList, FRHIComputeShader* Shader, const FShaderResourceParameter& Parameter, FRHIUnorderedAccessView* UAV)
{
	SetUAVParameterSafeShader(RHICmdList, Shader, Parameter, UAV);
}

FORCEINLINE void SetUAVParameter(FRHICommandList& RHICmdList, FRHIPixelShader* Shader, const FShaderResourceParameter& Parameter, FRHIUnorderedAccessView* UAV)
{
	SetUAVParameterSafeShader(RHICmdList, Shader, Parameter, UAV);
}


template<typename TRHICmdList>
inline bool SetUAVParameterIfCS(TRHICmdList& RHICmdList, FRHIVertexShader* Shader, const FShaderResourceParameter& UAVParameter, FRHIUnorderedAccessView* UAV)
{
	return false;
}

template<typename TRHICmdList>
inline bool SetUAVParameterIfCS(TRHICmdList& RHICmdList, FRHIPixelShader* Shader, const FShaderResourceParameter& UAVParameter, FRHIUnorderedAccessView* UAV)
{
	SetUAVParameter(RHICmdList, Shader, UAVParameter, UAV);
	return UAVParameter.IsBound();
}

template<typename TRHICmdList>
inline bool SetUAVParameterIfCS(TRHICmdList& RHICmdList, FRHIGeometryShader* Shader, const FShaderResourceParameter& UAVParameter, FRHIUnorderedAccessView* UAV)
{
	return false;
}

template<typename TRHICmdList>
inline bool SetUAVParameterIfCS(TRHICmdList& RHICmdList, FRHIComputeShader* Shader, const FShaderResourceParameter& UAVParameter, FRHIUnorderedAccessView* UAV)
{
	SetUAVParameter(RHICmdList, Shader, UAVParameter, UAV);
	return UAVParameter.IsBound();
}

template<typename TShaderRHIRef, typename TRHICmdList>
inline void FRWShaderParameter::SetBuffer(TRHICmdList& RHICmdList, const TShaderRHIRef& Shader, const FRWBuffer& RWBuffer) const
{
	if (!SetUAVParameterIfCS(RHICmdList, Shader, UAVParameter, RWBuffer.UAV))
	{
		SetSRVParameter(RHICmdList, Shader, SRVParameter, RWBuffer.SRV);
	}
}

template<typename TShaderRHIRef, typename TRHICmdList>
inline void FRWShaderParameter::SetBuffer(TRHICmdList& RHICmdList, const TShaderRHIRef& Shader, const FRWBufferStructured& RWBuffer) const
{
	if (!SetUAVParameterIfCS(RHICmdList, Shader, UAVParameter, RWBuffer.UAV))
	{
		SetSRVParameter(RHICmdList, Shader, SRVParameter, RWBuffer.SRV);
	}
}

template<typename TShaderRHIRef, typename TRHICmdList>
inline void FRWShaderParameter::SetTexture(TRHICmdList& RHICmdList, const TShaderRHIRef& Shader, FRHITexture* Texture, FRHIUnorderedAccessView* UAV) const
{
	if (!SetUAVParameterIfCS(RHICmdList, Shader, UAVParameter, UAV))
	{
		SetTextureParameter(RHICmdList, Shader, SRVParameter, Texture);
	}
}

template<typename TRHICmdList>
inline void FRWShaderParameter::SetUAV(TRHICmdList& RHICmdList, FRHIComputeShader* ComputeShader, FRHIUnorderedAccessView* UAV) const
{
	SetUAVParameter(RHICmdList, ComputeShader, UAVParameter, UAV);
}

template<typename TRHICmdList>
inline void FRWShaderParameter::UnsetUAV(TRHICmdList& RHICmdList, FRHIComputeShader* ComputeShader) const
{
	SetUAVParameter(RHICmdList, ComputeShader,UAVParameter,FUnorderedAccessViewRHIRef());
}


/** Sets the value of a shader uniform buffer parameter to a uniform buffer containing the struct. */
template<typename TShaderRHIRef, typename TRHICmdList>
UE_DEPRECATED(5.1, "Local uniform buffers are now deprecated. Use SetUniformBufferParameter instead.")
inline void SetLocalUniformBufferParameter(
	TRHICmdList& RHICmdList,
	const TShaderRHIRef& Shader,
	const FShaderUniformBufferParameter& Parameter,
	const FLocalUniformBuffer& LocalUniformBuffer
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());
	if(Parameter.IsBound())
	{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		RHICmdList.SetLocalShaderUniformBuffer(Shader, Parameter.GetBaseIndex(), LocalUniformBuffer);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
}

/** Sets the value of a shader uniform buffer parameter to a uniform buffer containing the struct. */
template<typename TShaderRHIRef, typename TRHICmdList>
inline void SetUniformBufferParameter(
	TRHICmdList& RHICmdList,
	const TShaderRHIRef& Shader,
	const FShaderUniformBufferParameter& Parameter,
	FRHIUniformBuffer* UniformBufferRHI
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());
	// If it is bound, we must set it so something valid
	checkSlow(!Parameter.IsBound() || UniformBufferRHI);
	if(Parameter.IsBound())
	{
		RHICmdList.SetShaderUniformBuffer( Shader, Parameter.GetBaseIndex(), UniformBufferRHI);
	}
}

/** Sets the value of a shader uniform buffer parameter to a uniform buffer containing the struct. */
template<typename TShaderRHIRef, typename TBufferStruct, typename TRHICmdList>
inline void SetUniformBufferParameter(
	TRHICmdList& RHICmdList,
	const TShaderRHIRef& Shader,
	const TShaderUniformBufferParameter<TBufferStruct>& Parameter,
	const TUniformBufferRef<TBufferStruct>& UniformBufferRef
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());
	// If it is bound, we must set it so something valid
	checkSlow(!Parameter.IsBound() || IsValidRef(UniformBufferRef));
	if(Parameter.IsBound())
	{
		RHICmdList.SetShaderUniformBuffer( Shader, Parameter.GetBaseIndex(), UniformBufferRef);
	}
}

/** Sets the value of a shader uniform buffer parameter to a uniform buffer containing the struct. */
template<typename TShaderRHIRef, typename TBufferStruct, typename TRHICmdList>
inline void SetUniformBufferParameter(
	TRHICmdList& RHICmdList,
	const TShaderRHIRef& Shader,
	const TShaderUniformBufferParameter<TBufferStruct>& Parameter,
	const TUniformBuffer<TBufferStruct>& UniformBuffer
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());
	// If it is bound, we must set it so something valid
	checkSlow(!Parameter.IsBound() || UniformBuffer.GetUniformBufferRHI());
	if (Parameter.IsBound())
	{
		RHICmdList.SetShaderUniformBuffer( Shader, Parameter.GetBaseIndex(), UniformBuffer.GetUniformBufferRHI());
	}
}

/** Sets the value of a shader uniform buffer parameter to a value of the struct. */
template<typename TShaderRHIRef,typename TBufferStruct>
inline void SetUniformBufferParameterImmediate(
	FRHICommandList& RHICmdList,
	const TShaderRHIRef& Shader,
	const TShaderUniformBufferParameter<TBufferStruct>& Parameter,
	const TBufferStruct& UniformBufferValue
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());
	if(Parameter.IsBound())
	{
		RHICmdList.SetShaderUniformBuffer(
			Shader,
			Parameter.GetBaseIndex(),
			RHICreateUniformBuffer(&UniformBufferValue, &TBufferStruct::StaticStructMetadata.GetLayout(), UniformBuffer_SingleDraw)
		);
	}
}

/** Sets the value of a shader uniform buffer parameter to a value of the struct. */
template<typename TShaderRHIRef,typename TBufferStruct, typename TRHICmdList>
inline void SetUniformBufferParameterImmediate(
	TRHICmdList& RHICmdList,
	const TShaderRHIRef& Shader,
	const TShaderUniformBufferParameter<TBufferStruct>& Parameter,
	const TBufferStruct& UniformBufferValue
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());
	if(Parameter.IsBound())
	{
		RHICmdList.SetShaderUniformBuffer(
			Shader,
			Parameter.GetBaseIndex(),
			RHICreateUniformBuffer(&UniformBufferValue, &TBufferStruct::StaticStructMetadata.GetLayout(), UniformBuffer_SingleDraw)
		);
	}
}