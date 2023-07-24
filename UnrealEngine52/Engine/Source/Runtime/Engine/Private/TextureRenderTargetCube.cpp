// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureRenderTargetCube.cpp: UTextureRenderTargetCube implementation
=============================================================================*/

#include "Engine/TextureRenderTargetCube.h"
#include "HAL/LowLevelMemStats.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "UObject/UnrealType.h"
#include "UnrealEngine.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "Engine/TextureCube.h"
#if WITH_EDITOR
#include "Components/SceneCaptureComponentCube.h"
#include "UObject/UObjectIterator.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(TextureRenderTargetCube)

/*-----------------------------------------------------------------------------
	UTextureRenderTargetCube
-----------------------------------------------------------------------------*/

UTextureRenderTargetCube::UTextureRenderTargetCube(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHDR = true;
	ClearColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
	OverrideFormat = PF_Unknown;
	bForceLinearGamma = true;
}


void UTextureRenderTargetCube::Init(uint32 InSizeX, EPixelFormat InFormat)
{
	check(InSizeX > 0);
	check(!(InSizeX % GPixelFormats[InFormat].BlockSizeX));
	check(FTextureRenderTargetResource::IsSupportedFormat(InFormat));

	// set required size/format
	SizeX = InSizeX;
	OverrideFormat = InFormat;

	// Recreate the texture's resource.
	UpdateResource();
}

void UTextureRenderTargetCube::InitAutoFormat(uint32 InSizeX)
{
	check(InSizeX > 0);
	check(!(InSizeX % GPixelFormats[GetFormat()].BlockSizeX));
	check(FTextureRenderTargetResource::IsSupportedFormat(GetFormat()));

	// set required size/format
	SizeX = InSizeX;

	// Recreate the texture's resource.
	UpdateResource();
}

void UTextureRenderTargetCube::UpdateResourceImmediate(bool bClearRenderTarget/*=true*/)
{
	if (GetResource())
	{
		FTextureRenderTargetCubeResource* InResource = static_cast<FTextureRenderTargetCubeResource*>(GetResource());
		ENQUEUE_RENDER_COMMAND(UpdateResourceImmediate)(
			[InResource, bClearRenderTarget](FRHICommandListImmediate& RHICmdList)
			{
				InResource->UpdateDeferredResource(RHICmdList, bClearRenderTarget);
			}
		);
	}
}

void UTextureRenderTargetCube::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	// Calculate size based on format.
	EPixelFormat Format = GetFormat();
	int32 BlockSizeX	= GPixelFormats[Format].BlockSizeX;
	int32 BlockSizeY	= GPixelFormats[Format].BlockSizeY;
	int32 BlockBytes	= GPixelFormats[Format].BlockBytes;
	int32 NumBlocksX	= (SizeX + BlockSizeX - 1) / BlockSizeX;
	int32 NumBlocksY	= (SizeX + BlockSizeY - 1) / BlockSizeY;
	int32 NumBytes	= NumBlocksX * NumBlocksY * BlockBytes * 6;

	CumulativeResourceSize.AddUnknownMemoryBytes(NumBytes);
}


FTextureResource* UTextureRenderTargetCube::CreateResource()
{
	return new FTextureRenderTargetCubeResource(this);
}


EMaterialValueType UTextureRenderTargetCube::GetMaterialType() const
{
	return MCT_TextureCube;
}

#if WITH_EDITOR
void UTextureRenderTargetCube::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	constexpr int32 MaxSize = 2048;

	EPixelFormat Format = GetFormat();
	SizeX = FMath::Clamp<int32>(SizeX - (SizeX % GPixelFormats[Format].BlockSizeX),1,MaxSize);

	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Notify any scene capture components that point to this texture that they may need to refresh
	static const FName SizeXName = GET_MEMBER_NAME_CHECKED(UTextureRenderTargetCube, SizeX);

	if (PropertyChangedEvent.GetPropertyName() == SizeXName)
	{
		for (TObjectIterator<USceneCaptureComponentCube> It; It; ++It)
		{
			USceneCaptureComponentCube* SceneCaptureComponent = *It;
			if (SceneCaptureComponent->TextureTarget == this)
			{
				// During interactive edits, time is paused, so the Tick function which normally handles capturing isn't called, and we
				// need a manual refresh.  We also need a refresh if the capture doesn't happen automatically every frame.
				if ((PropertyChangedEvent.ChangeType & EPropertyChangeType::Interactive) || !SceneCaptureComponent->bCaptureEveryFrame)
				{
					SceneCaptureComponent->CaptureSceneDeferred();
				}
			}
		}
	}
}
#endif // WITH_EDITOR


void UTextureRenderTargetCube::PostLoad()
{
	Super::PostLoad();

	if (!FPlatformProperties::SupportsWindowedMode())
	{
		// clamp the render target size in order to avoid reallocating the scene render targets
		SizeX = FMath::Min<int32>(SizeX,FMath::Min<int32>(GSystemResolution.ResX, GSystemResolution.ResY));
	}
}


FString UTextureRenderTargetCube::GetDesc()
{
	return FString::Printf( TEXT("Render to Texture Cube %dx%d[%s]"), SizeX, SizeX, GPixelFormats[GetFormat()].Name);
}

UTextureCube* UTextureRenderTargetCube::ConstructTextureCube(
  UObject* ObjOuter,
  const FString& NewTexName,
  EObjectFlags InFlags
)
{
	UTextureCube* Result = NULL;
#if WITH_EDITOR
	// Check render target size is valid and power of two.
	if (SizeX != 0 && !(SizeX & (SizeX - 1)))
	{
		const EPixelFormat PixelFormat = GetFormat();
		ETextureSourceFormat TextureFormat = TSF_Invalid;
		switch (PixelFormat)
		{
			case PF_FloatRGBA:
				TextureFormat = TSF_RGBA16F;
				break;
			case PF_B8G8R8A8:
				TextureFormat = TSF_BGRA8;
				break;
			default:
				return nullptr;
		}

		// The r2t resource will be needed to read its surface contents
		FTextureRenderTargetCubeResource* CubeResource = (FTextureRenderTargetCubeResource*)GameThread_GetRenderTargetResource();
		if (CubeResource && TextureFormat != TSF_Invalid)
		{
			// create the cube texture
			Result = NewObject<UTextureCube>(ObjOuter, FName(*NewTexName), InFlags);

			bool bSRGB = true;
			// if render target gamma used was 1.0 then disable SRGB for the static texture
			if (FMath::Abs(CubeResource->GetDisplayGamma() - 1.0f) < UE_KINDA_SMALL_NUMBER)
			{
				bSRGB = false;
			}

			Result->Source.Init(SizeX, SizeX, 6, 1, TextureFormat);

			int32 MipSize = CalculateImageBytes(SizeX, SizeX, 0, PixelFormat);
			uint8* SliceData = Result->Source.LockMip(0);

			switch (TextureFormat)
			{
				case TSF_BGRA8:
				{
					TArray<FColor> OutputBuffer;
					for (int32 SliceIndex = 0; SliceIndex < 6; SliceIndex++)
					{
						if (CubeResource->ReadPixels(OutputBuffer, FReadSurfaceDataFlags(RCM_UNorm, (ECubeFace)SliceIndex)))
						{
							FMemory::Memcpy((FColor*)(SliceData + SliceIndex * MipSize), OutputBuffer.GetData(), MipSize);
						}
					}
				}
				break;
				case TSF_RGBA16F:
				{
					TArray<FFloat16Color> OutputBuffer;
					for (int32 SliceIndex = 0; SliceIndex < 6; SliceIndex++)
					{
						if (CubeResource->ReadPixels(OutputBuffer, FReadSurfaceDataFlags(RCM_UNorm, (ECubeFace)SliceIndex)))
						{
							FMemory::Memcpy((FFloat16Color*)(SliceData + SliceIndex * MipSize), OutputBuffer.GetData(), MipSize);
						}
					}
				}
				break;
			}

			Result->Source.UnlockMip(0);
			Result->SRGB = bSRGB;
			// If HDR source image then choose HDR compression settings..
			Result->CompressionSettings = TextureFormat == TSF_RGBA16F ? TextureCompressionSettings::TC_HDR : TextureCompressionSettings::TC_Default;
			// Default to no mip generation for cube render target captures.
			Result->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
			Result->PostEditChange();
		}
	}
#endif // #if WITH_EDITOR
	return Result;
}

/*-----------------------------------------------------------------------------
	FTextureRenderTargetCubeResource
-----------------------------------------------------------------------------*/

/**
 * Initializes the RHI render target resources used by this resource.
 * Called when the resource is initialized, or when reseting all RHI resources.
 * This is only called by the rendering thread.
 */
void FTextureRenderTargetCubeResource::InitDynamicRHI()
{
	LLM_SCOPED_TAG_WITH_OBJECT_IN_SET(Owner->GetOutermost(), ELLMTagSet::Assets);

	if(Owner->SizeX > 0)
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
				FRHITextureCreateDesc::CreateCube(TEXT("FTextureRenderTargetCubeResource"))
				.SetExtent(Owner->SizeX)
				.SetFormat(Owner->GetFormat())
				.SetNumMips(Owner->GetNumMips())
				.SetFlags(TexCreateFlags | ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
				.SetClearValue(FClearValueBinding(Owner->ClearColor))
				.SetInitialState(ERHIAccess::SRVMask);

			TextureRHI = RHICreateTexture(Desc);
		}

		SetGPUMask(FRHIGPUMask::All());

		if (EnumHasAnyFlags(TexCreateFlags, ETextureCreateFlags::UAV))
		{
			UnorderedAccessViewRHI = RHICreateUnorderedAccessView(TextureRHI);
		}

		RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, TextureRHI);

		// Create the RHI target surface used for rendering to
		{
			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create2D(TEXT("FTextureRenderTargetCubeResource"))
				.SetExtent(Owner->SizeX, Owner->SizeX)
				.SetFormat(Owner->GetFormat())
				.SetNumMips(Owner->GetNumMips())
				.SetFlags(TexCreateFlags | ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
				.SetClearValue(FClearValueBinding(Owner->ClearColor))
				.SetInitialState(ERHIAccess::SRVMask);

			RenderTargetTextureRHI = RHICreateTexture(Desc);
		}

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
void FTextureRenderTargetCubeResource::ReleaseDynamicRHI()
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
void FTextureRenderTargetCubeResource::UpdateDeferredResource(FRHICommandListImmediate& RHICmdList, bool bClearRenderTarget/*=true*/)
{
	LLM_SCOPED_TAG_WITH_OBJECT_IN_SET(Owner->GetOutermost(), ELLMTagSet::Assets);

	RemoveFromDeferredUpdateList();

	if (!bClearRenderTarget)
	{
		return;
	}

	RHICmdList.Transition(FRHITransitionInfo(RenderTargetTextureRHI, ERHIAccess::Unknown, ERHIAccess::RTV));

	ClearRenderTarget(RHICmdList, RenderTargetTextureRHI);

	RHICmdList.Transition({
		FRHITransitionInfo(RenderTargetTextureRHI, ERHIAccess::RTV, ERHIAccess::CopySrc),
		FRHITransitionInfo(TextureRHI, ERHIAccess::Unknown, ERHIAccess::CopyDest)
	});

	FRHICopyTextureInfo CopyInfo;

	for(int32 FaceIdx = CubeFace_PosX; FaceIdx < CubeFace_MAX; FaceIdx++)
	{
		CopyInfo.DestSliceIndex = FaceIdx;
		RHICmdList.CopyTexture(RenderTargetTextureRHI, TextureRHI, CopyInfo);
	}

	RHICmdList.Transition({
		FRHITransitionInfo(RenderTargetTextureRHI, ERHIAccess::CopySrc, ERHIAccess::SRVMask),
		FRHITransitionInfo(TextureRHI, ERHIAccess::CopyDest, ERHIAccess::SRVMask)
	});
}

/** 
 * @return width of target
 */
uint32 FTextureRenderTargetCubeResource::GetSizeX() const
{
	return Owner->SizeX;
}

/** 
 * @return height of target
 */
uint32 FTextureRenderTargetCubeResource::GetSizeY() const
{
	return Owner->SizeX;
}

/** 
 * @return dimensions of target surface
 */
FIntPoint FTextureRenderTargetCubeResource::GetSizeXY() const
{
	return FIntPoint(Owner->SizeX, Owner->SizeX);
}

float FTextureRenderTargetCubeResource::GetDisplayGamma() const
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


/**
* Copy the texels of a single face of a cube texture into an array.
* @param OutImageData - RGBA8 values will be stored in this array.
* @param InFlags - read flags. ensure cubeface member has been set.
* @param InRect - Rectangle of texels to copy. Empty InRect (0,0,0,0) defaults to the whole surface size.
* @return True if the read succeeded.
*/
bool FTextureRenderTargetCubeResource::ReadPixels(TArray< FColor >& OutImageData, FReadSurfaceDataFlags InFlags, FIntRect InRect)
{
	if (InRect == FIntRect(0, 0, 0, 0))
	{
		InRect = FIntRect(0, 0, GetSizeXY().X, GetSizeXY().Y);
	}

	// Read the render target surface data back.
	struct FReadSurfaceContext
	{
		FTextureRenderTargetCubeResource* SrcRenderTarget;
		TArray<FColor>* OutData;
		FIntRect Rect;
		FReadSurfaceDataFlags Flags;
	};

	OutImageData.Reset();
	FReadSurfaceContext Context =
	{
		this,
		&OutImageData,
		InRect,
		InFlags
	};

	ENQUEUE_RENDER_COMMAND(ReadSurfaceCommand)(
		[Context](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.ReadSurfaceData(
				Context.SrcRenderTarget->TextureRHI,
				Context.Rect,
				*Context.OutData,
				Context.Flags
			);
		});
	FlushRenderingCommands();

	return true;
}

/**
* Copy the texels of a single face of a cube texture into an array.
* @param OutImageData - float16 values will be stored in this array.
* @param InFlags - read flags. ensure cubeface member has been set.
* @param InRect - Rectangle of texels to copy. Empty InRect (0,0,0,0) defaults to the whole surface size.
* @return True if the read succeeded.
*/
bool FTextureRenderTargetCubeResource::ReadPixels(TArray<FFloat16Color>& OutImageData, FReadSurfaceDataFlags InFlags, FIntRect InRect)
{
	if (InRect == FIntRect(0, 0, 0, 0))
	{
		InRect = FIntRect(0, 0, GetSizeXY().X, GetSizeXY().Y);
	}
	// Read the render target surface data back.
	struct FReadSurfaceFloatContext
	{
		FTextureRenderTargetCubeResource* SrcRenderTarget;
		TArray<FFloat16Color>* OutData;
		FIntRect Rect;
		ECubeFace CubeFace;
	};

	FReadSurfaceFloatContext Context =
	{
		this,
		&OutImageData,
		InRect,
		InFlags.GetCubeFace()
	};

	ENQUEUE_RENDER_COMMAND(ReadSurfaceFloatCommand)(
		[Context](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.ReadSurfaceFloatData(
				Context.SrcRenderTarget->TextureRHI,
				Context.Rect,
				*Context.OutData,
				Context.CubeFace,
				0,
				0
			);
	});

	FlushRenderingCommands();

	return true;
}

