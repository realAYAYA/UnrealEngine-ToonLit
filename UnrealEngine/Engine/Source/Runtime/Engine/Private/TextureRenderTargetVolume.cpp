// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureRenderTargetVolume.cpp: UTextureRenderTargetVolume implementation
=============================================================================*/

#include "Engine/TextureRenderTargetVolume.h"
#include "RenderUtils.h"
#include "TextureRenderTargetVolumeResource.h"
#include "UnrealEngine.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "Engine/VolumeTexture.h"
#include "ClearQuad.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TextureRenderTargetVolume)

/*-----------------------------------------------------------------------------
	UTextureRenderTargetVolume
-----------------------------------------------------------------------------*/

UTextureRenderTargetVolume::UTextureRenderTargetVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHDR = true;
	ClearColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
	OverrideFormat = PF_Unknown;
	bForceLinearGamma = true;
}

void UTextureRenderTargetVolume::Init(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, EPixelFormat InFormat)
{
	check((InSizeX > 0) && (InSizeY > 0) && (InSizeZ > 0));
	check(!(InSizeX % GPixelFormats[InFormat].BlockSizeX));
	check(!(InSizeY % GPixelFormats[InFormat].BlockSizeY));
	check(!(InSizeZ % GPixelFormats[InFormat].BlockSizeZ));
	//check(FTextureRenderTargetResource::IsSupportedFormat(InFormat));

	// set required size/format
	SizeX = InSizeX;
	SizeY = InSizeY;
	SizeZ = InSizeZ;
	OverrideFormat = InFormat;

	// Recreate the texture's resource.
	UpdateResource();
}

void UTextureRenderTargetVolume::InitAutoFormat(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ)
{
	check((InSizeX > 0) && (InSizeY > 0) && (InSizeZ > 0));
	check(!(InSizeX % GPixelFormats[GetFormat()].BlockSizeX));
	check(!(InSizeY % GPixelFormats[GetFormat()].BlockSizeY));
	check(!(InSizeZ % GPixelFormats[GetFormat()].BlockSizeZ));
	//check(FTextureRenderTargetResource::IsSupportedFormat(GetFormat()));

	// set required size/format
	SizeX = InSizeX;
	SizeY = InSizeY;
	SizeZ = InSizeZ;

	// Recreate the texture's resource.
	UpdateResource();
}

void UTextureRenderTargetVolume::UpdateResourceImmediate(bool bClearRenderTarget/*=true*/)
{
	if (GetResource())
	{
		FTextureRenderTargetVolumeResource* InResource = static_cast<FTextureRenderTargetVolumeResource*>(GetResource());
		ENQUEUE_RENDER_COMMAND(UpdateResourceImmediate)(
			[InResource, bClearRenderTarget](FRHICommandListImmediate& RHICmdList)
			{
				InResource->UpdateDeferredResource(RHICmdList, bClearRenderTarget);
			}
		);
	}
}
void UTextureRenderTargetVolume::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	// Calculate size based on format.
	const EPixelFormat Format = GetFormat();
	const int32 BlockSizeX	= GPixelFormats[Format].BlockSizeX;
	const int32 BlockSizeY	= GPixelFormats[Format].BlockSizeY;
	const int32 BlockSizeZ	= GPixelFormats[Format].BlockSizeZ;
	const int32 BlockBytes	= GPixelFormats[Format].BlockBytes;
	const int32 NumBlocksX	= (SizeX + BlockSizeX - 1) / BlockSizeX;
	const int32 NumBlocksY	= (SizeY + BlockSizeY - 1) / BlockSizeY;
	const int32 NumBlocksZ	= (SizeZ + BlockSizeY - 1) / BlockSizeZ;
	const int32 NumBytes	= NumBlocksX * NumBlocksY * NumBlocksZ * BlockBytes;

	CumulativeResourceSize.AddUnknownMemoryBytes(NumBytes);
}

FTextureResource* UTextureRenderTargetVolume::CreateResource()
{
	return new FTextureRenderTargetVolumeResource(this);
}

EMaterialValueType UTextureRenderTargetVolume::GetMaterialType() const
{
	return MCT_VolumeTexture;
}

#if WITH_EDITOR
void UTextureRenderTargetVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	constexpr int32 MaxSize = 2048;

	EPixelFormat Format = GetFormat();
	SizeX = FMath::Clamp<int32>(SizeX - (SizeX % GPixelFormats[Format].BlockSizeX), 1, MaxSize);
	SizeY = FMath::Clamp<int32>(SizeY - (SizeY % GPixelFormats[Format].BlockSizeY), 1, MaxSize);
	SizeZ = FMath::Clamp<int32>(SizeZ - (SizeZ % GPixelFormats[Format].BlockSizeZ), 1, MaxSize);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UTextureRenderTargetVolume::PostLoad()
{
	Super::PostLoad();

	if (!FPlatformProperties::SupportsWindowedMode())
	{
		// clamp the render target size in order to avoid reallocating the scene render targets
		SizeX = FMath::Min<int32>(SizeX, FMath::Min<int32>(GSystemResolution.ResX, GSystemResolution.ResY));
		SizeY = FMath::Min<int32>(SizeY, FMath::Min<int32>(GSystemResolution.ResX, GSystemResolution.ResY));
		SizeZ = FMath::Min<int32>(SizeZ, FMath::Min<int32>(GSystemResolution.ResX, GSystemResolution.ResY));
	}
}

FString UTextureRenderTargetVolume::GetDesc()
{
	return FString::Printf( TEXT("Render to Texture Volume %dx%d[%s]"), SizeX, SizeX, GPixelFormats[GetFormat()].Name);
}

UVolumeTexture* UTextureRenderTargetVolume::ConstructTextureVolume(UObject* ObjOuter, const FString& NewTexName, EObjectFlags InFlags)
{
#if WITH_EDITOR
	if (SizeX == 0 || SizeY == 0 || SizeZ == 0)
	{
		return nullptr;
	}


	const EPixelFormat PixelFormat = GetFormat();
	ETextureSourceFormat TextureFormat = TSF_Invalid;
	switch (PixelFormat)
	{
		case PF_R32_FLOAT:
		case PF_FloatRGBA:
			TextureFormat = TSF_RGBA16F;
			break;
	}

	if (TextureFormat == TSF_Invalid)
	{
		return nullptr;
	}

	FTextureRenderTargetVolumeResource* TextureResource = (FTextureRenderTargetVolumeResource*)GameThread_GetRenderTargetResource();
	if (TextureResource == nullptr)
	{
		return nullptr;
	}

	// Create texture
	UVolumeTexture* VolumeTexture = NewObject<UVolumeTexture>(ObjOuter, FName(*NewTexName), InFlags);

	bool bSRGB = true;
	// if render target gamma used was 1.0 then disable SRGB for the static texture
	if (FMath::Abs(TextureResource->GetDisplayGamma() - 1.0f) < UE_KINDA_SMALL_NUMBER)
	{
		bSRGB = false;
	}
	VolumeTexture->Source.Init(SizeX, SizeX, SizeZ, 1, TextureFormat);

	const int32 SrcMipSize = CalculateImageBytes(SizeX, SizeX, 1, PixelFormat);
	const int32 DstMipSize = CalculateImageBytes(SizeX, SizeX, 1, PF_FloatRGBA);
	uint8* SliceData = VolumeTexture->Source.LockMip(0);
	switch (TextureFormat)
	{
		case TSF_RGBA16F:
		{
			for (int i = 0; i < SizeZ; ++i)
			{
				TArray<FFloat16Color> OutputBuffer;
				FReadSurfaceDataFlags ReadSurfaceDataFlags(RCM_UNorm);
				if (TextureResource->ReadPixels(OutputBuffer, i))
				{
					FMemory::Memcpy((FFloat16Color*)(SliceData + i * DstMipSize), OutputBuffer.GetData(), DstMipSize);
				}
			}
			break;
		}

		default:
			// Missing conversion from PF -> TSF
			check(false);
			break;
	}

	VolumeTexture->Source.UnlockMip(0);
	VolumeTexture->SRGB = bSRGB;
	// If HDR source image then choose HDR compression settings..
	VolumeTexture->CompressionSettings = TextureFormat == TSF_RGBA16F ? TextureCompressionSettings::TC_HDR : TextureCompressionSettings::TC_Default; //-V547 - future proofing
	// Default to no mip generation for cube render target captures.
	VolumeTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
	VolumeTexture->PostEditChange();

	return VolumeTexture;
#else
	return nullptr;
#endif // #if WITH_EDITOR
}

/*-----------------------------------------------------------------------------
	FTextureRenderTargetVolumeResource
-----------------------------------------------------------------------------*/

/**
 * Initializes the RHI render target resources used by this resource.
 * Called when the resource is initialized, or when reseting all RHI resources.
 * This is only called by the rendering thread.
 */
void FTextureRenderTargetVolumeResource::InitDynamicRHI()
{
	if((Owner->SizeX > 0) && (Owner->SizeY > 0) && (Owner->SizeZ > 0))
	{
		bool bIsSRGB = true;

		// if render target gamma used was 1.0 then disable SRGB for the static texture
		if(FMath::Abs(GetDisplayGamma() - 1.0f) < UE_KINDA_SMALL_NUMBER)
		{
			bIsSRGB = false;
		}

		// Create the RHI texture. Only one mip is used and the texture is targetable for resolve.
		ETextureCreateFlags TexCreateFlags = bIsSRGB ? TexCreate_SRGB : TexCreate_None;

		if (Owner->bCanCreateUAV)
		{
			TexCreateFlags |= TexCreate_UAV;
		}

		{
			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create3D(TEXT("FTextureRenderTargetVolumeResource"))
				.SetExtent(Owner->SizeX, Owner->SizeY)
				.SetDepth(Owner->SizeZ)
				.SetFormat(Owner->GetFormat())
				.SetNumMips(Owner->GetNumMips())
				.SetFlags(TexCreateFlags | ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
				.SetClearValue(FClearValueBinding(Owner->ClearColor));

			TextureRHI = RenderTargetTextureRHI = RHICreateTexture(Desc);
		}

		if (EnumHasAnyFlags(TexCreateFlags, TexCreate_UAV))
		{
			UnorderedAccessViewRHI = RHICreateUnorderedAccessView(TextureRHI);
		}

		RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI,TextureRHI);

		// Can't set this as it's a texture 2D
		//RenderTargetTextureRHI = VolumeSurfaceRHI;

		AddToDeferredUpdateList(true);
	}

	// Create the sampler state RHI resource.
	FSamplerStateInitializerRHI SamplerStateInitializer
	(
		(ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(Owner),
		AM_Clamp,
		AM_Clamp,
		AM_Clamp
	);
	SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);
}

/**
 * Release the RHI render target resources used by this resource.
 * Called when the resource is released, or when reseting all RHI resources.
 * This is only called by the rendering thread.
 */
void FTextureRenderTargetVolumeResource::ReleaseDynamicRHI()
{
	// release the FTexture RHI resources here as well
	ReleaseRHI();

	RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, nullptr);
	RenderTargetTextureRHI.SafeRelease();

	// remove from global list of deferred clears
	RemoveFromDeferredUpdateList();
}

/**
 * Updates (resolves) the render target texture.
 * Optionally clears each face of the render target to green.
 * This is only called by the rendering thread.
 */
void FTextureRenderTargetVolumeResource::UpdateDeferredResource(FRHICommandListImmediate& RHICmdList, bool bClearRenderTarget/*=true*/)
{
	if (bClearRenderTarget)
	{
		RHICmdList.Transition(FRHITransitionInfo(TextureRHI, ERHIAccess::Unknown, ERHIAccess::RTV));
		ClearRenderTarget(RHICmdList, TextureRHI);
		RHICmdList.Transition(FRHITransitionInfo(TextureRHI, ERHIAccess::RTV, ERHIAccess::SRVMask));
	}
}

/** 
 * @return width of target
 */
uint32 FTextureRenderTargetVolumeResource::GetSizeX() const
{
	return Owner->SizeX;
}

/** 
 * @return height of target
 */
uint32 FTextureRenderTargetVolumeResource::GetSizeY() const
{
	return Owner->SizeX;
}

/** 
 * @return dimensions of target surface
 */
FIntPoint FTextureRenderTargetVolumeResource::GetSizeXY() const
{
	return FIntPoint(Owner->SizeX, Owner->SizeX);
}

float FTextureRenderTargetVolumeResource::GetDisplayGamma() const
{
	if(Owner->TargetGamma > UE_KINDA_SMALL_NUMBER * 10.0f)
	{
		return Owner->TargetGamma;
	}
	EPixelFormat Format = Owner->GetFormat();
	if(Format == PF_FloatRGB || Format == PF_FloatRGBA || Owner->bForceLinearGamma)
	{
		return 1.0f;
	}
	return FTextureRenderTargetResource::GetDisplayGamma();
}

bool FTextureRenderTargetVolumeResource::ReadPixels(TArray<FColor>& OutImageData, int32 InDepthSlice, FIntRect InRect)
{
	if (InRect == FIntRect(0, 0, 0, 0))
	{
		InRect = FIntRect(0, 0, GetSizeXY().X, GetSizeXY().Y);
	}

	OutImageData.Reset();

	ENQUEUE_RENDER_COMMAND(ReadSurfaceCommand)
	(
		[RenderTarget_RT=this, OutData_RT=&OutImageData, Rect_RT=InRect, DepthSlice_RT=InDepthSlice, bSRGB_RT= bSRGB](FRHICommandListImmediate& RHICmdList)
		{
			TArray<FFloat16Color> TempData;
			RHICmdList.Read3DSurfaceFloatData(RenderTarget_RT->TextureRHI, Rect_RT, FIntPoint(DepthSlice_RT, DepthSlice_RT + 1), TempData);
			for (const FFloat16Color& SrcColor : TempData)
			{
				OutData_RT->Emplace(FLinearColor(SrcColor).ToFColor(bSRGB_RT));
			}
		}
	);
	FlushRenderingCommands();

	return true;
}

bool FTextureRenderTargetVolumeResource::ReadPixels(TArray<FFloat16Color>& OutImageData, int32 InDepthSlice, FIntRect InRect)
{
	if (InRect == FIntRect(0, 0, 0, 0))
	{
		InRect = FIntRect(0, 0, GetSizeXY().X, GetSizeXY().Y);
	}

	OutImageData.Reset();

	ENQUEUE_RENDER_COMMAND(ReadSurfaceCommand)
	(
		[RenderTarget_RT=this, OutData_RT=&OutImageData, Rect_RT=InRect, DepthSlice_RT=InDepthSlice](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.Read3DSurfaceFloatData(RenderTarget_RT->TextureRHI, Rect_RT, FIntPoint(DepthSlice_RT, DepthSlice_RT+1), *OutData_RT);
		}
	);
	FlushRenderingCommands();

	return true;
}

