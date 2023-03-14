// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterMacros.h"
#include "ShaderParameters.h"
#include "RenderUtils.h"
#include "RHIStaticStates.h"
#include "RenderGraphResources.h"

class FShaderParameterMap;

namespace GlobalDistanceField
{
	/** Must match global distance field shaders. */
	const int32 MaxClipmaps = 6;
}

class FGlobalDistanceFieldParameterData
{
public:

	FGlobalDistanceFieldParameterData()
	{
		FPlatformMemory::Memzero(this, sizeof(FGlobalDistanceFieldParameterData));
	}

	FVector4f CenterAndExtent[GlobalDistanceField::MaxClipmaps];
	FVector4f WorldToUVAddAndMul[GlobalDistanceField::MaxClipmaps];
	FVector4f MipWorldToUVScale[GlobalDistanceField::MaxClipmaps];
	FVector4f MipWorldToUVBias[GlobalDistanceField::MaxClipmaps];
	float MipFactor;
	float MipTransition;
	FRHITexture* PageAtlasTexture;
	FRHITexture* CoverageAtlasTexture;
	TRefCountPtr<FRDGPooledBuffer> PageObjectGridBuffer;
	FRHITexture* PageTableTexture;
	FRHITexture* MipTexture;
	int32 ClipmapSizeInPages;
	FVector InvPageAtlasSize;
	FVector InvCoverageAtlasSize;
	int32 MaxPageNum;
	float GlobalDFResolution;
	float MaxDFAOConeDistance;
	int32 NumGlobalSDFClipmaps;
};

BEGIN_SHADER_PARAMETER_STRUCT(FGlobalDistanceFieldParameters2, )
	SHADER_PARAMETER_TEXTURE(Texture3D, GlobalDistanceFieldPageAtlasTexture)
	SHADER_PARAMETER_TEXTURE(Texture3D, GlobalDistanceFieldCoverageAtlasTexture)
	SHADER_PARAMETER_TEXTURE(Texture3D<uint>, GlobalDistanceFieldPageTableTexture)
	SHADER_PARAMETER_TEXTURE(Texture3D, GlobalDistanceFieldMipTexture)
	SHADER_PARAMETER_ARRAY(FVector4f, GlobalVolumeCenterAndExtent, [GlobalDistanceField::MaxClipmaps])
	SHADER_PARAMETER_ARRAY(FVector4f, GlobalVolumeWorldToUVAddAndMul, [GlobalDistanceField::MaxClipmaps])
	SHADER_PARAMETER_ARRAY(FVector4f, GlobalDistanceFieldMipWorldToUVScale, [GlobalDistanceField::MaxClipmaps])
	SHADER_PARAMETER_ARRAY(FVector4f, GlobalDistanceFieldMipWorldToUVBias, [GlobalDistanceField::MaxClipmaps])
	SHADER_PARAMETER(float, GlobalDistanceFieldMipFactor)
	SHADER_PARAMETER(float, GlobalDistanceFieldMipTransition)
	SHADER_PARAMETER(int32, GlobalDistanceFieldClipmapSizeInPages)
	SHADER_PARAMETER(FVector3f, GlobalDistanceFieldInvPageAtlasSize)
	SHADER_PARAMETER(FVector3f, GlobalDistanceFieldInvCoverageAtlasSize)
	SHADER_PARAMETER(float, GlobalVolumeDimension)
	SHADER_PARAMETER(float, GlobalVolumeTexelSize)
	SHADER_PARAMETER(float, MaxGlobalDFAOConeDistance)
	SHADER_PARAMETER(uint32, NumGlobalSDFClipmaps)
	SHADER_PARAMETER(float, CoveredExpandSurfaceScale)
	SHADER_PARAMETER(float, NotCoveredExpandSurfaceScale)
	SHADER_PARAMETER(float, NotCoveredMinStepScale)
	SHADER_PARAMETER(float, DitheredTransparencyStepThreshold)
	SHADER_PARAMETER(float, DitheredTransparencyTraceThreshold)
END_SHADER_PARAMETER_STRUCT()

FGlobalDistanceFieldParameters2 SetupGlobalDistanceFieldParameters(const FGlobalDistanceFieldParameterData& ParameterData);

class FGlobalDistanceFieldParameters
{
	DECLARE_INLINE_TYPE_LAYOUT(FGlobalDistanceFieldParameters, NonVirtual);
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		GlobalDistanceFieldPageAtlasTexture.Bind(ParameterMap, TEXT("GlobalDistanceFieldPageAtlasTexture"));
		GlobalDistanceFieldPageTableTexture.Bind(ParameterMap, TEXT("GlobalDistanceFieldPageTableTexture"));
		GlobalDistanceFieldMipTexture.Bind(ParameterMap, TEXT("GlobalDistanceFieldMipTexture"));
		GlobalDistanceFieldMipFactor.Bind(ParameterMap, TEXT("GlobalDistanceFieldMipFactor"));
		GlobalDistanceFieldMipTransition.Bind(ParameterMap, TEXT("GlobalDistanceFieldMipTransition"));
		GlobalVolumeCenterAndExtent.Bind(ParameterMap, TEXT("GlobalVolumeCenterAndExtent"));
		GlobalVolumeWorldToUVAddAndMul.Bind(ParameterMap, TEXT("GlobalVolumeWorldToUVAddAndMul"));
		GlobalDistanceFieldMipWorldToUVScale.Bind(ParameterMap, TEXT("GlobalDistanceFieldMipWorldToUVScale"));
		GlobalDistanceFieldMipWorldToUVBias.Bind(ParameterMap, TEXT("GlobalDistanceFieldMipWorldToUVBias"));
		GlobalDistanceFieldClipmapSizeInPages.Bind(ParameterMap, TEXT("GlobalDistanceFieldClipmapSizeInPages"));
		GlobalDistanceFieldInvPageAtlasSize.Bind(ParameterMap, TEXT("GlobalDistanceFieldInvPageAtlasSize"));
		GlobalVolumeDimension.Bind(ParameterMap,TEXT("GlobalVolumeDimension"));
		GlobalVolumeTexelSize.Bind(ParameterMap,TEXT("GlobalVolumeTexelSize"));
		MaxGlobalDFAOConeDistance.Bind(ParameterMap,TEXT("MaxGlobalDFAOConeDistance"));
		NumGlobalSDFClipmaps.Bind(ParameterMap,TEXT("NumGlobalSDFClipmaps"));
	}

	bool IsBound() const
	{
		return GlobalVolumeCenterAndExtent.IsBound() || GlobalVolumeWorldToUVAddAndMul.IsBound();
	}

	friend FArchive& operator<<(FArchive& Ar,FGlobalDistanceFieldParameters& Parameters)
	{
		Ar << Parameters.GlobalDistanceFieldPageAtlasTexture;
		Ar << Parameters.GlobalDistanceFieldPageTableTexture;
		Ar << Parameters.GlobalDistanceFieldMipTexture;
		Ar << Parameters.GlobalVolumeCenterAndExtent;
		Ar << Parameters.GlobalVolumeWorldToUVAddAndMul;
		Ar << Parameters.GlobalDistanceFieldMipWorldToUVScale;
		Ar << Parameters.GlobalDistanceFieldMipWorldToUVBias;
		Ar << Parameters.GlobalDistanceFieldMipFactor;
		Ar << Parameters.GlobalDistanceFieldMipTransition;
		Ar << Parameters.GlobalDistanceFieldClipmapSizeInPages;
		Ar << Parameters.GlobalDistanceFieldInvPageAtlasSize;
		Ar << Parameters.GlobalVolumeDimension;
		Ar << Parameters.GlobalVolumeTexelSize;
		Ar << Parameters.MaxGlobalDFAOConeDistance;
		Ar << Parameters.NumGlobalSDFClipmaps;
		return Ar;
	}

	template<typename ShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FGlobalDistanceFieldParameterData& ParameterData) const
	{
		if (IsBound())
		{
			SetTextureParameter(RHICmdList, ShaderRHI, GlobalDistanceFieldPageAtlasTexture, ParameterData.PageAtlasTexture ? ParameterData.PageAtlasTexture : GBlackVolumeTexture->TextureRHI.GetReference());
			SetTextureParameter(RHICmdList, ShaderRHI, GlobalDistanceFieldPageTableTexture, ParameterData.PageTableTexture ? ParameterData.PageTableTexture : GBlackVolumeTexture->TextureRHI.GetReference());
			SetTextureParameter(RHICmdList, ShaderRHI, GlobalDistanceFieldMipTexture, ParameterData.MipTexture ? ParameterData.MipTexture : GBlackVolumeTexture->TextureRHI.GetReference());

			SetShaderValueArray(RHICmdList, ShaderRHI, GlobalVolumeCenterAndExtent, ParameterData.CenterAndExtent, GlobalDistanceField::MaxClipmaps);
			SetShaderValueArray(RHICmdList, ShaderRHI, GlobalVolumeWorldToUVAddAndMul, ParameterData.WorldToUVAddAndMul, GlobalDistanceField::MaxClipmaps);
			SetShaderValueArray(RHICmdList, ShaderRHI, GlobalDistanceFieldMipWorldToUVScale, ParameterData.MipWorldToUVScale, GlobalDistanceField::MaxClipmaps);
			SetShaderValueArray(RHICmdList, ShaderRHI, GlobalDistanceFieldMipWorldToUVBias, ParameterData.MipWorldToUVBias, GlobalDistanceField::MaxClipmaps);
			SetShaderValue(RHICmdList, ShaderRHI, GlobalDistanceFieldMipFactor, ParameterData.MipFactor);
			SetShaderValue(RHICmdList, ShaderRHI, GlobalDistanceFieldMipTransition, ParameterData.MipTransition);
			SetShaderValue(RHICmdList, ShaderRHI, GlobalDistanceFieldClipmapSizeInPages, ParameterData.ClipmapSizeInPages);
			SetShaderValue(RHICmdList, ShaderRHI, GlobalDistanceFieldInvPageAtlasSize, (FVector3f)ParameterData.InvPageAtlasSize);
			SetShaderValue(RHICmdList, ShaderRHI, GlobalVolumeDimension, ParameterData.GlobalDFResolution);
			SetShaderValue(RHICmdList, ShaderRHI, GlobalVolumeTexelSize, 1.0f / ParameterData.GlobalDFResolution);
			SetShaderValue(RHICmdList, ShaderRHI, MaxGlobalDFAOConeDistance, ParameterData.MaxDFAOConeDistance);
			SetShaderValue(RHICmdList, ShaderRHI, NumGlobalSDFClipmaps, ParameterData.NumGlobalSDFClipmaps);
		}
	}

private:

	LAYOUT_FIELD(FShaderResourceParameter, GlobalDistanceFieldPageAtlasTexture)
	LAYOUT_FIELD(FShaderResourceParameter, GlobalDistanceFieldPageTableTexture)
	LAYOUT_FIELD(FShaderResourceParameter, GlobalDistanceFieldMipTexture)
	LAYOUT_FIELD(FShaderParameter, GlobalVolumeCenterAndExtent)
	LAYOUT_FIELD(FShaderParameter, GlobalVolumeWorldToUVAddAndMul)
	LAYOUT_FIELD(FShaderParameter, GlobalDistanceFieldMipWorldToUVScale)
	LAYOUT_FIELD(FShaderParameter, GlobalDistanceFieldMipWorldToUVBias)
	LAYOUT_FIELD(FShaderParameter, GlobalDistanceFieldMipFactor)
	LAYOUT_FIELD(FShaderParameter, GlobalDistanceFieldMipTransition)
	LAYOUT_FIELD(FShaderParameter, GlobalDistanceFieldClipmapSizeInPages)	
	LAYOUT_FIELD(FShaderParameter, GlobalDistanceFieldInvPageAtlasSize)
	LAYOUT_FIELD(FShaderParameter, GlobalVolumeDimension)
	LAYOUT_FIELD(FShaderParameter, GlobalVolumeTexelSize)
	LAYOUT_FIELD(FShaderParameter, MaxGlobalDFAOConeDistance)
	LAYOUT_FIELD(FShaderParameter, NumGlobalSDFClipmaps)
};
