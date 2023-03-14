// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingIESLightProfiles.h"
#include "SceneRendering.h"
#include "CopyTextureShaders.h"

#if RHI_RAYTRACING

void FIESLightProfileResource::BuildIESLightProfilesTexture(FRHICommandListImmediate& RHICmdList, const TArray<UTextureLightProfile*, SceneRenderingAllocator>& NewIESProfilesArray)
{
	// Rebuild 2D texture that contains one IES light profile per row

	check(IsInRenderingThread());

	bool NeedsRebuild = false;
	if (NewIESProfilesArray.Num() != IESTextureData.Num())
	{
		NeedsRebuild = true;
		IESTextureData.SetNum(NewIESProfilesArray.Num(), true);
	}
	else
	{
		for (int32 i = 0; i < IESTextureData.Num(); ++i)
		{
			if (IESTextureData[i] != NewIESProfilesArray[i])
			{
				NeedsRebuild = true;
				break;
			}
		}
	}

	uint32 NewArraySize = NewIESProfilesArray.Num();

	if (!NeedsRebuild || NewArraySize == 0)
	{
		return;
	}

	if (!DefaultTexture)
	{
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("RTDefaultIESProfile"), AllowedIESProfileWidth, 1, AllowedIESProfileFormat)
			.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV);

		DefaultTexture = RHICreateTexture(Desc);
		FUnorderedAccessViewRHIRef UAV = RHICreateUnorderedAccessView(DefaultTexture, 0);

		RHICmdList.Transition(FRHITransitionInfo(DefaultTexture, ERHIAccess::Unknown, ERHIAccess::UAVCompute));
		RHICmdList.ClearUAVFloat(UAV, FVector4f(1.0f, 1.0f, 1.0f, 1.0f));
		RHICmdList.Transition(FRHITransitionInfo(DefaultTexture, ERHIAccess::UAVCompute, ERHIAccess::SRVMask));
	}

	if (!AtlasTexture || AtlasTexture->GetSizeY() != NewArraySize)
	{
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("RTIESProfileAtlas"), AllowedIESProfileWidth, NewArraySize, AllowedIESProfileFormat)
			.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV);

		AtlasTexture = RHICreateTexture(Desc);
		AtlasUAV = RHICreateUnorderedAccessView(AtlasTexture, 0);
	}

	CopyTextureCS::DispatchContext DispatchContext;
	TShaderRef<FCopyTextureCS> Shader = FCopyTextureCS::SelectShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), 
		ECopyTextureResourceType::Texture2D, // SrcType
		ECopyTextureResourceType::Texture2D, // DstType
		ECopyTextureValueType::Float,
		DispatchContext); // out DispatchContext
	FRHIComputeShader* ShaderRHI = Shader.GetComputeShader();

	RHICmdList.Transition(FRHITransitionInfo(AtlasUAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute));
	SetComputePipelineState(RHICmdList, ShaderRHI);
	RHICmdList.SetUAVParameter(ShaderRHI, Shader->GetDstResourceParam().GetBaseIndex(), AtlasUAV);
	RHICmdList.BeginUAVOverlap(AtlasUAV);
	for (uint32 ProfileIndex = 0; ProfileIndex < NewArraySize; ++ProfileIndex)
	{
		IESTextureData[ProfileIndex] = NewIESProfilesArray[ProfileIndex];
		const UTextureLightProfile* LightProfileTexture = IESTextureData[ProfileIndex];

		FTextureRHIRef ProfileTexture; 
		if (IsIESTextureFormatValid(LightProfileTexture))
		{
			ProfileTexture = LightProfileTexture->GetResource()->TextureRHI;
		}
		else
		{
			ProfileTexture = DefaultTexture;
		}

		RHICmdList.SetShaderTexture(ShaderRHI, Shader->GetSrcResourceParam().GetBaseIndex(), ProfileTexture);
		Shader->Dispatch(RHICmdList, DispatchContext,
			FIntVector(0, 0, 0), // SrcOffset
			FIntVector(0, ProfileIndex, 0), // DstOffset
			FIntVector(AllowedIESProfileWidth, 1, 1));
	}
	RHICmdList.EndUAVOverlap(AtlasUAV);
	RHICmdList.Transition(FRHITransitionInfo(AtlasUAV, ERHIAccess::UAVCompute, ERHIAccess::SRVMask));
}

bool FIESLightProfileResource::IsIESTextureFormatValid(const UTextureLightProfile* Texture) const
{
	if (Texture
		&& Texture->GetResource()
		&& Texture->GetResource()->TextureRHI
		&& Texture->GetPlatformData()
		&& Texture->GetPlatformData()->PixelFormat == AllowedIESProfileFormat
		&& Texture->GetPlatformData()->Mips.Num() == 1
		&& Texture->GetPlatformData()->Mips[0].SizeX == AllowedIESProfileWidth
		//#dxr_todo: UE-70840 anisotropy in IES files is ignored so far (to support that, we should not store one IES profile per row but use more than one row per profile in that case)
		&& Texture->GetPlatformData()->Mips[0].SizeY == 1
		)
	{
		return true;
	}
	else
	{
		return false;
	}
}

#endif
