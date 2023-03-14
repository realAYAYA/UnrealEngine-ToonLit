// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureRenderTarget2DArray.cpp: UTextureRenderTarget2DArray implementation
=============================================================================*/

#include "Engine/TextureRenderTarget2DArray.h"
#include "RenderUtils.h"
#include "TextureRenderTarget2DArrayResource.h"
#include "UnrealEngine.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "Engine/Texture2DArray.h"
#include "ClearQuad.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TextureRenderTarget2DArray)

/*-----------------------------------------------------------------------------
	UTextureRenderTarget2DArray
-----------------------------------------------------------------------------*/

UTextureRenderTarget2DArray::UTextureRenderTarget2DArray(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHDR = true;
	ClearColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
	OverrideFormat = PF_Unknown;
	bForceLinearGamma = true;
}

void UTextureRenderTarget2DArray::Init(uint32 InSizeX, uint32 InSizeY, uint32 InSlices, EPixelFormat InFormat)
{
	check((InSizeX > 0) && (InSizeY > 0) && (InSlices > 0));
	check(!(InSizeX % GPixelFormats[InFormat].BlockSizeX));
	check(!(InSizeY % GPixelFormats[InFormat].BlockSizeY));
	//check(FTextureRenderTargetResource::IsSupportedFormat(InFormat));

	// set required size/format
	SizeX = InSizeX;
	SizeY = InSizeY;
	Slices = InSlices;
	OverrideFormat = InFormat;

	// Recreate the texture's resource.
	UpdateResource();
}

void UTextureRenderTarget2DArray::InitAutoFormat(uint32 InSizeX, uint32 InSizeY, uint32 InSlices)
{
	check((InSizeX > 0) && (InSizeY > 0) && (InSlices > 0));
	check(!(InSizeX % GPixelFormats[GetFormat()].BlockSizeX));
	check(!(InSizeY % GPixelFormats[GetFormat()].BlockSizeY));
	//check(FTextureRenderTargetResource::IsSupportedFormat(GetFormat()));

	// set required size/format
	SizeX = InSizeX;
	SizeY = InSizeY;
	Slices = InSlices;

	// Recreate the texture's resource.
	UpdateResource();
}

void UTextureRenderTarget2DArray::UpdateResourceImmediate(bool bClearRenderTarget/*=true*/)
{
	if (GetResource())
	{
		FTextureRenderTarget2DArrayResource* InResource = static_cast<FTextureRenderTarget2DArrayResource*>(GetResource());
		ENQUEUE_RENDER_COMMAND(UpdateResourceImmediate)(
			[InResource, bClearRenderTarget](FRHICommandListImmediate& RHICmdList)
			{
				InResource->UpdateDeferredResource(RHICmdList, bClearRenderTarget);
			}
		);
	}
}
void UTextureRenderTarget2DArray::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	// Calculate size based on format.
	const EPixelFormat Format = GetFormat();
	const int64 BlockSizeX	= GPixelFormats[Format].BlockSizeX;
	const int64 BlockSizeY	= GPixelFormats[Format].BlockSizeY;
	const int64 BlockBytes	= GPixelFormats[Format].BlockBytes;
	const int64 NumBlocksX	= (SizeX + BlockSizeX - 1) / BlockSizeX;
	const int64 NumBlocksY	= (SizeY + BlockSizeY - 1) / BlockSizeY;
	const int64 NumBytes	= NumBlocksX * NumBlocksY * Slices * BlockBytes;

	CumulativeResourceSize.AddUnknownMemoryBytes(NumBytes);
}

FTextureResource* UTextureRenderTarget2DArray::CreateResource()
{
	return new FTextureRenderTarget2DArrayResource(this);
}

EMaterialValueType UTextureRenderTarget2DArray::GetMaterialType() const
{
	return MCT_Texture2DArray;
}

#if WITH_EDITOR
void UTextureRenderTarget2DArray::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	constexpr int32 MaxSize = 2048;

	EPixelFormat Format = GetFormat();
	SizeX = FMath::Clamp<int32>(SizeX - (SizeX % GPixelFormats[Format].BlockSizeX), 1, MaxSize);
	SizeY = FMath::Clamp<int32>(SizeY - (SizeY % GPixelFormats[Format].BlockSizeY), 1, MaxSize);
	Slices = FMath::Clamp<int32>(Slices, 1, MaxSize);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UTextureRenderTarget2DArray::PostLoad()
{
	Super::PostLoad();

	if (!FPlatformProperties::SupportsWindowedMode())
	{
		// clamp the render target size in order to avoid reallocating the scene render targets
		SizeX = FMath::Min<int32>(SizeX, FMath::Min<int32>(GSystemResolution.ResX, GSystemResolution.ResY));
		SizeY = FMath::Min<int32>(SizeY, FMath::Min<int32>(GSystemResolution.ResX, GSystemResolution.ResY));
		Slices = FMath::Min<int32>(Slices, FMath::Min<int32>(GSystemResolution.ResX, GSystemResolution.ResY));
	}
}

FString UTextureRenderTarget2DArray::GetDesc()
{
	return FString::Printf( TEXT("Render to Texture 2DArray %dx%d[%s]"), SizeX, SizeX, GPixelFormats[GetFormat()].Name);
}

UTexture2DArray* UTextureRenderTarget2DArray::ConstructTexture2DArray(UObject* ObjOuter, const FString& NewTexName, EObjectFlags InFlags)
{
#if WITH_EDITOR
	if (SizeX == 0 || SizeY == 0 || Slices == 0)
	{
		return nullptr;
	}

	const EPixelFormat PixelFormat = GetFormat();
	ETextureSourceFormat TextureFormat = TSF_Invalid;
	switch (PixelFormat)
	{
		case PF_FloatRGBA:
			TextureFormat = TSF_RGBA16F;
			break;
	}

	if (TextureFormat == TSF_Invalid)
	{
		return nullptr;
	}

	FTextureRenderTarget2DArrayResource* TextureResource = (FTextureRenderTarget2DArrayResource*)GameThread_GetRenderTargetResource();
	if (TextureResource == nullptr)
	{
		return nullptr;
	}

	// Create texture
	UTexture2DArray* Texture2DArray = NewObject<UTexture2DArray>(ObjOuter, FName(*NewTexName), InFlags);

	bool bSRGB = true;
	// if render target gamma used was 1.0 then disable SRGB for the static texture
	if (FMath::Abs(TextureResource->GetDisplayGamma() - 1.0f) < UE_KINDA_SMALL_NUMBER)
	{
		bSRGB = false;
	}
	Texture2DArray->Source.Init(SizeX, SizeX, Slices, 1, TextureFormat);

	const int32 SrcMipSize = CalculateImageBytes(SizeX, SizeY, 1, PixelFormat);
	const int32 DstMipSize = CalculateImageBytes(SizeX, SizeY, 1, PF_FloatRGBA);
	uint8* SliceData = Texture2DArray->Source.LockMip(0);
	switch (TextureFormat)
	{
		case TSF_RGBA16F:
		{
			for (int i = 0; i < Slices; ++i)
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

	Texture2DArray->Source.UnlockMip(0);
	Texture2DArray->SRGB = bSRGB;
	// If HDR source image then choose HDR compression settings..
	Texture2DArray->CompressionSettings = TextureFormat == TSF_RGBA16F ? TextureCompressionSettings::TC_HDR : TextureCompressionSettings::TC_Default; //-V547 - future proofing
	// Default to no mip generation for cube render target captures.
	Texture2DArray->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
	Texture2DArray->PostEditChange();

	return Texture2DArray;
#else
	return nullptr;
#endif // #if WITH_EDITOR
}

/*-----------------------------------------------------------------------------
	FTextureRenderTarget2DArrayResource
-----------------------------------------------------------------------------*/

/**
 * Initializes the RHI render target resources used by this resource.
 * Called when the resource is initialized, or when reseting all RHI resources.
 * This is only called by the rendering thread.
 */
void FTextureRenderTarget2DArrayResource::InitDynamicRHI()
{
	if((Owner->SizeX > 0) && (Owner->SizeY > 0) && (Owner->Slices > 0))
	{
		bool bIsSRGB = true;

		// if render target gamma used was 1.0 then disable SRGB for the static texture
		if(FMath::Abs(GetDisplayGamma() - 1.0f) < UE_KINDA_SMALL_NUMBER)
		{
			bIsSRGB = false;
		}

		// Create the RHI texture. Only one mip is used and the texture is targetable for resolve.
		ETextureCreateFlags TexCreateFlags = bIsSRGB ? ETextureCreateFlags::SRGB : ETextureCreateFlags::None;
		if (Owner->bCanCreateUAV)
		{
			TexCreateFlags |= ETextureCreateFlags::UAV;
		}

		{
			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create2DArray(TEXT("FTextureRenderTarget2DArrayResource"))
				.SetExtent(Owner->SizeX, Owner->SizeY)
				.SetArraySize(Owner->Slices)
				.SetFormat(Owner->GetFormat())
				.SetNumMips(Owner->GetNumMips())
				.SetFlags(TexCreateFlags | ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
				.SetClearValue(FClearValueBinding(Owner->ClearColor))
				.SetInitialState(ERHIAccess::SRVMask);

			TextureRHI = RHICreateTexture(Desc);
		}

		if (EnumHasAnyFlags(TexCreateFlags, ETextureCreateFlags::UAV))
		{
			UnorderedAccessViewRHI = RHICreateUnorderedAccessView(TextureRHI);
		}

		RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, TextureRHI);
		RenderTargetTextureRHI = TextureRHI;

		AddToDeferredUpdateList(true);
	}

	// Create the sampler state RHI resource.
	FSamplerStateInitializerRHI SamplerStateInitializer
	(
		(ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(Owner),
		AM_Wrap,
		AM_Wrap,
		AM_Wrap
	);
	SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);
}

/**
 * Release the RHI render target resources used by this resource.
 * Called when the resource is released, or when reseting all RHI resources.
 * This is only called by the rendering thread.
 */
void FTextureRenderTarget2DArrayResource::ReleaseDynamicRHI()
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
void FTextureRenderTarget2DArrayResource::UpdateDeferredResource(FRHICommandListImmediate& RHICmdList, bool bClearRenderTarget/*=true*/)
{
	if (!bClearRenderTarget)
	{
		return;
	}

	RHICmdList.Transition(FRHITransitionInfo(TextureRHI, ERHIAccess::Unknown, ERHIAccess::RTV));
	ClearRenderTarget(RHICmdList, TextureRHI);
	RHICmdList.Transition(FRHITransitionInfo(TextureRHI, ERHIAccess::RTV, ERHIAccess::SRVMask));
}

/** 
 * @return width of target
 */
uint32 FTextureRenderTarget2DArrayResource::GetSizeX() const
{
	return Owner->SizeX;
}

/** 
 * @return height of target
 */
uint32 FTextureRenderTarget2DArrayResource::GetSizeY() const
{
	return Owner->SizeX;
}

/** 
 * @return dimensions of target surface
 */
FIntPoint FTextureRenderTarget2DArrayResource::GetSizeXY() const
{
	return FIntPoint(Owner->SizeX, Owner->SizeX);
}

float FTextureRenderTarget2DArrayResource::GetDisplayGamma() const
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

bool FTextureRenderTarget2DArrayResource::ReadPixels(TArray<FColor>& OutImageData, int32 InSlice, FIntRect InRect)
{
	if (InRect == FIntRect(0, 0, 0, 0))
	{
		InRect = FIntRect(0, 0, GetSizeXY().X, GetSizeXY().Y);
	}

	OutImageData.Reset();

	ENQUEUE_RENDER_COMMAND(ReadSurfaceCommand)
	(
		[RenderTarget_RT=this, OutData_RT=&OutImageData, Rect_RT=InRect, Slice_RT=InSlice, bSRGB_RT= bSRGB](FRHICommandListImmediate& RHICmdList)
		{
			TArray<FFloat16Color> TempData;
			RHICmdList.ReadSurfaceFloatData(RenderTarget_RT->TextureRHI, Rect_RT, TempData, (ECubeFace)0, Slice_RT, 0);
			for (const FFloat16Color& SrcColor : TempData)
			{
				OutData_RT->Emplace(FLinearColor(SrcColor).ToFColor(bSRGB_RT));
			}
		}
	);
	FlushRenderingCommands();

	return true;
}

bool FTextureRenderTarget2DArrayResource::ReadPixels(TArray<FFloat16Color>& OutImageData, int32 InSlice, FIntRect InRect)
{
	if (InRect == FIntRect(0, 0, 0, 0))
	{
		InRect = FIntRect(0, 0, GetSizeXY().X, GetSizeXY().Y);
	}

	OutImageData.Reset();

	ENQUEUE_RENDER_COMMAND(ReadSurfaceCommand)
	(
		[RenderTarget_RT=this, OutData_RT=&OutImageData, Rect_RT=InRect, Slice_RT=InSlice](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.ReadSurfaceFloatData(RenderTarget_RT->TextureRHI, Rect_RT, *OutData_RT, (ECubeFace)0, Slice_RT, 0);
		}
	);
	FlushRenderingCommands();

	return true;
}

