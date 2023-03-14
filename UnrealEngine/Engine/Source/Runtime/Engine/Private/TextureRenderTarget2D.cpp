// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureRenderTarget2D.cpp: UTextureRenderTarget2D implementation
=============================================================================*/

#include "Engine/TextureRenderTarget2D.h"
#include "Misc/MessageDialog.h"
#include "TextureResource.h"
#include "Engine/Texture2D.h"
#include "UnrealEngine.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "UObject/RenderingObjectVersion.h"
#include "GenerateMips.h"
#include "RenderGraphUtils.h"
#if WITH_EDITOR
#include "Components/SceneCaptureComponent2D.h"
#include "UObject/UObjectIterator.h"
#endif

int32 GTextureRenderTarget2DMaxSizeX = 999999999;
int32 GTextureRenderTarget2DMaxSizeY = 999999999;

/*-----------------------------------------------------------------------------
	UTextureRenderTarget2D
-----------------------------------------------------------------------------*/

UTextureRenderTarget2D::UTextureRenderTarget2D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SizeX = 1;
	SizeY = 1;
	bHDR_DEPRECATED = true;
	RenderTargetFormat = RTF_RGBA16f;
	bAutoGenerateMips = false;
	NumMips = 0;
	ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
	OverrideFormat = PF_Unknown;
	bForceLinearGamma = true;
	MipsSamplerFilter = Filter;
	MipsAddressU = TA_Clamp;
	MipsAddressV = TA_Clamp;
}

FTextureResource* UTextureRenderTarget2D::CreateResource()
{
	UWorld* World = GetWorld();

	if (bAutoGenerateMips)
	{
		NumMips = FMath::FloorLog2(FMath::Max(SizeX, SizeY)) + 1;

		if (RHIRequiresComputeGenerateMips())
		{
			bCanCreateUAV = 1;
		}
	}
	else
	{
		NumMips = 1;
	}
 
	FTextureRenderTarget2DResource* Result = new FTextureRenderTarget2DResource(this);
	return Result;
}

uint32 UTextureRenderTarget2D::CalcTextureMemorySizeEnum(ETextureMipCount Enum) const
{
	// Calculate size based on format.  All mips are resident on render targets so we always return the same value.
	EPixelFormat Format = GetFormat();
	int32 BlockSizeX = GPixelFormats[Format].BlockSizeX;
	int32 BlockSizeY = GPixelFormats[Format].BlockSizeY;
	int32 BlockBytes = GPixelFormats[Format].BlockBytes;
	int32 NumBlocksX = (SizeX + BlockSizeX - 1) / BlockSizeX;
	int32 NumBlocksY = (SizeY + BlockSizeY - 1) / BlockSizeY;
	int32 NumBytes = NumBlocksX * NumBlocksY * BlockBytes;
	return NumBytes;
}

EMaterialValueType UTextureRenderTarget2D::GetMaterialType() const
{
	return MCT_Texture2D;
}

void UTextureRenderTarget2D::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	int32 NumBytes = CalcTextureMemorySizeEnum(TMC_AllMips);

	CumulativeResourceSize.AddUnknownMemoryBytes(NumBytes);
}

void UTextureRenderTarget2D::InitCustomFormat( uint32 InSizeX, uint32 InSizeY, EPixelFormat InOverrideFormat, bool bInForceLinearGamma )
{
	check(InSizeX > 0 && InSizeY > 0);
	check(FTextureRenderTargetResource::IsSupportedFormat(InOverrideFormat));

	// set required size/format
	SizeX = InSizeX;
	SizeY = InSizeY;
	OverrideFormat = InOverrideFormat;
	bForceLinearGamma = bInForceLinearGamma;

	if (!ensureMsgf(SizeX >= 0 && SizeX <= 65536, TEXT("Invalid SizeX=%u for RenderTarget %s"), SizeX, *GetName()))
	{
		SizeX = 1;
	}

	if (!ensureMsgf(SizeY >= 0 && SizeY <= 65536, TEXT("Invalid SizeY=%u for RenderTarget %s"), SizeY, *GetName()))
	{
		SizeY = 1;
	}

	// Recreate the texture's resource.
	UpdateResource();
}

void UTextureRenderTarget2D::InitAutoFormat(uint32 InSizeX, uint32 InSizeY)
{
	check(InSizeX > 0 && InSizeY > 0);

	// set required size
	SizeX = InSizeX;
	SizeY = InSizeY;

	// Recreate the texture's resource.
	UpdateResource();
}

void UTextureRenderTarget2D::ResizeTarget(uint32 InSizeX, uint32 InSizeY)
{
	if (SizeX != InSizeX || SizeY != InSizeY)
	{
		SizeX = InSizeX;
		SizeY = InSizeY;
		if (bAutoGenerateMips)
		{
			NumMips = FMath::FloorLog2(FMath::Max(SizeX, SizeY)) + 1;
		}

		if (GetResource())
		{
			FTextureRenderTarget2DResource* InResource = static_cast<FTextureRenderTarget2DResource*>(GetResource());
			int32 NewSizeX = SizeX;
			int32 NewSizeY = SizeY;
			ENQUEUE_RENDER_COMMAND(ResizeRenderTarget)(
				[InResource, NewSizeX, NewSizeY](FRHICommandListImmediate& RHICmdList)
				{
					InResource->Resize(NewSizeX, NewSizeY);
					InResource->UpdateDeferredResource(RHICmdList, true);
				}
			);
		}
		else
		{
			UpdateResource();
		}
	}
}

void UTextureRenderTarget2D::UpdateResourceImmediate(bool bClearRenderTarget/*=true*/)
{
	if (GetResource())
	{
		FTextureRenderTarget2DResource* InResource = static_cast<FTextureRenderTarget2DResource*>(GetResource());
		ENQUEUE_RENDER_COMMAND(UpdateResourceImmediate)(
			[InResource, bClearRenderTarget](FRHICommandListImmediate& RHICmdList)
			{
				InResource->UpdateDeferredResource(RHICmdList, bClearRenderTarget);
			}
		);
	}
}

#if WITH_EDITOR
void UTextureRenderTarget2D::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	EPixelFormat Format = GetFormat();

	const int32 WarnSize = 2048; 

	if (SizeX > WarnSize || SizeY > WarnSize)
	{
		const float MemoryMb = SizeX * SizeY * GPixelFormats[Format].BlockBytes / 1024.0f / 1024.0f;
		FNumberFormattingOptions FloatFormat;
		FloatFormat.SetMaximumFractionalDigits(1);
		FText Message = FText::Format( NSLOCTEXT("TextureRenderTarget2D", "LargeTextureRenderTarget2DWarning", "A TextureRenderTarget2D of size {0}x{1} will use {2}Mb, which may result in extremely poor performance or an Out Of Video Memory crash.\nAre you sure?"), FText::AsNumber(SizeX), FText::AsNumber(SizeY), FText::AsNumber(MemoryMb, &FloatFormat));
		const EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgType::YesNo, Message);
	
		if (Choice == EAppReturnType::No)
		{
			SizeX = FMath::Clamp<int32>(SizeX,1,WarnSize);
			SizeY = FMath::Clamp<int32>(SizeY,1,WarnSize);
		}
	}

	const int32 MaxSize = 8192; 
	
	SizeX = FMath::Clamp<int32>(SizeX - (SizeX % GPixelFormats[Format].BlockSizeX),1,MaxSize);
	SizeY = FMath::Clamp<int32>(SizeY - (SizeY % GPixelFormats[Format].BlockSizeY),1,MaxSize);

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTextureRenderTarget2D, RenderTargetFormat))
	{
		if (RenderTargetFormat == RTF_RGBA8_SRGB)
		{
			bForceLinearGamma = false;
		}
		else
		{
			bForceLinearGamma = true;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

    // SRGB may have been changed by Super, reset it since we prefer to honor explicit user choice
	SRGB = IsSRGB();

	// Notify any scene capture components that point to this texture that they may need to refresh
	static const FName SizeXName = GET_MEMBER_NAME_CHECKED(UTextureRenderTarget2D, SizeX);
	static const FName SizeYName = GET_MEMBER_NAME_CHECKED(UTextureRenderTarget2D, SizeY);

	if ((PropertyChangedEvent.GetPropertyName() == SizeXName || PropertyChangedEvent.GetPropertyName() == SizeYName))
	{
		for (TObjectIterator<USceneCaptureComponent2D> It; It; ++It)
		{
			USceneCaptureComponent2D* SceneCaptureComponent = *It;
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

void UTextureRenderTarget2D::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

	if (Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::AddedTextureRenderTargetFormats)
	{
		RenderTargetFormat = bHDR_DEPRECATED ? RTF_RGBA16f : RTF_RGBA8;
	}

	if (Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::ExplicitSRGBSetting)
	{
		float DisplayGamme = 2.2f;
		EPixelFormat Format = GetFormat();

		if (TargetGamma > UE_KINDA_SMALL_NUMBER * 10.0f)
		{
			DisplayGamme = TargetGamma;
		}
		else if (Format == PF_FloatRGB || Format == PF_FloatRGBA || bForceLinearGamma)
		{
			DisplayGamme = 1.0f;
		}

		// This is odd behavior to apply the sRGB gamma correction when target gamma is not 1.0f, but this
		// is to maintain old behavior and users won't have to change content.
		if (RenderTargetFormat == RTF_RGBA8 && FMath::Abs(DisplayGamme - 1.0f) > UE_KINDA_SMALL_NUMBER)
		{
			RenderTargetFormat = RTF_RGBA8_SRGB;
			SRGB = true;
		}
	}
}

void UTextureRenderTarget2D::PostLoad()
{
	float OriginalSizeX = SizeX;
	float OriginalSizeY = SizeY;

	SizeX = FMath::Min<int32>(SizeX, GTextureRenderTarget2DMaxSizeX);
	SizeY = FMath::Min<int32>(SizeY, GTextureRenderTarget2DMaxSizeY);

	// Maintain aspect ratio if clamped
	if( SizeX != OriginalSizeX || SizeY != OriginalSizeY )
	{
		float ScaleX = SizeX / OriginalSizeX;
		float ScaleY = SizeY / OriginalSizeY;
		
		if( ScaleX < ScaleY )
		{
			SizeY = OriginalSizeY * ScaleX;
		}
		else
		{
			SizeX = OriginalSizeX * ScaleY;
		}
	}
	
	Super::PostLoad();
}


FString UTextureRenderTarget2D::GetDesc()	
{
	// size and format string
	return FString::Printf( TEXT("Render to Texture %dx%d[%s]"), SizeX, SizeY, GPixelFormats[GetFormat()].Name );
}

UTexture2D* UTextureRenderTarget2D::ConstructTexture2D(UObject* Outer, const FString& NewTexName, EObjectFlags InObjectFlags, uint32 Flags, TArray<uint8>* AlphaOverride)
{
	UTexture2D* Result = nullptr;

#if WITH_EDITOR
	// Check render target size is valid.
	const bool bIsValidSize = (SizeX != 0 && SizeY != 0);

	// The render to texture resource will be needed to read its surface contents
	FRenderTarget* RenderTarget = GameThread_GetRenderTargetResource();

	const ETextureSourceFormat TextureFormat = GetTextureFormatForConversionToTexture2D();

	// exit if source is not compatible.
	if (bIsValidSize == false || RenderTarget == nullptr || TextureFormat == TSF_Invalid)
	{
		return Result;
	}

	// Create the 2d texture
	Result = NewObject<UTexture2D>(Outer, FName(*NewTexName), InObjectFlags);
	
	UpdateTexture2D(Result, TextureFormat, Flags, AlphaOverride);

	// if render target gamma used was 1.0 then disable SRGB for the static texture
	// note: UTextureRenderTarget2D also has an explicit SRGB flag in the UTexture parent class
	//	  these are NOT correctly kept in sync
	//		I see SRGB = 1 but Gamma = 1.0
	//	 see also IsSRGB() which is yet another query that has different ideas
	//	furthermore, float formats do not support anything but Linear gamma
	float Gamma = RenderTarget->GetDisplayGamma();
	if (FMath::Abs(Gamma - 1.0f) < UE_KINDA_SMALL_NUMBER)
	{
		Flags &= ~CTF_SRGB;
	}

	Result->SRGB = (Flags & CTF_SRGB) != 0;

	Result->MipGenSettings = TMGS_FromTextureGroup;

	if ((Flags & CTF_AllowMips) == 0)
	{
		Result->MipGenSettings = TMGS_NoMipmaps;
	}

	if (Flags & CTF_Compress)
	{
		// Set compression options.
		Result->DeferCompression = (Flags & CTF_DeferCompression) ? true : false;
	}
	else
	{
		// Disable compression
		Result->CompressionNone = true;
		Result->DeferCompression = false;
	}
	Result->PostEditChange();
#endif

	return Result;
}

ETextureSourceFormat UTextureRenderTarget2D::GetTextureFormatForConversionToTexture2D() const
{
	const EPixelFormat PixelFormat = GetFormat();
	ETextureSourceFormat TextureFormat = TSF_Invalid;
	switch (PixelFormat)
	{
	case PF_B8G8R8A8:
		TextureFormat = TSF_BGRA8;
		break;
	case PF_FloatRGBA:
		TextureFormat = TSF_RGBA16F;
		break;
	case PF_G8:
		TextureFormat = TSF_G8;
		break;
	default:
	{
		FText InvalidFormatMessage = NSLOCTEXT("TextureRenderTarget2D", "UnsupportedFormatRenderTarget2DWarning", "Unsupported format when creating Texture2D from TextureRenderTarget2D. Supported formats are B8G8R8A8, FloatRGBA and G8.");
		FMessageDialog::Open(EAppMsgType::Ok, InvalidFormatMessage);
	}
	}

	return TextureFormat;
}

void UTextureRenderTarget2D::UpdateTexture2D(UTexture2D* InTexture2D, ETextureSourceFormat InTextureFormat, uint32 Flags, TArray<uint8>* AlphaOverride)
{
	UpdateTexture2D(InTexture2D, InTextureFormat, Flags, AlphaOverride, FTextureChangingDelegate());
}

void UTextureRenderTarget2D::UpdateTexture2D(UTexture2D* InTexture2D, ETextureSourceFormat InTextureFormat, uint32 Flags, TArray<uint8>* AlphaOverride, FTextureChangingDelegate TextureChangingDelegate)
{
#if WITH_EDITOR
	const EPixelFormat PixelFormat = GetFormat();
	TextureCompressionSettings CompressionSettingsForTexture = PixelFormat == EPixelFormat::PF_FloatRGBA ? TC_HDR : TC_Default;

	const int32 NbSlices = 1;
	const int32 NbMips = 1;

	bool bTextureChanging = false;
	
	bTextureChanging |= InTexture2D->Source.GetSizeX() != SizeX;
	bTextureChanging |= InTexture2D->Source.GetSizeY() != SizeY;
	bTextureChanging |= InTexture2D->Source.GetNumSlices() != NbSlices;
	bTextureChanging |= InTexture2D->Source.GetNumMips() != NbMips;
	bTextureChanging |= InTexture2D->Source.GetFormat() != InTextureFormat;
	bTextureChanging |= InTexture2D->CompressionSettings != CompressionSettingsForTexture;
	
	uint32 OldDataHash = 0;

	// Hashing the content is only really useful if none of the other texture settings have changed.
	const bool bHashContent = !bTextureChanging;
	if (bHashContent)
	{
		const uint8* OldData = InTexture2D->Source.LockMipReadOnly(0);
		const int32 OldDataSize = InTexture2D->Source.CalcMipSize(0);
		OldDataHash = FCrc::MemCrc32(OldData, OldDataSize);
		InTexture2D->Source.UnlockMip(0);
	}

	// Temp storage that will be used for the texture init.
	TArray<uint8> NewDataGrayscale;
	TArray<FColor> NewDataColor;
	TArray<FFloat16Color> NewDataFloat16Color;

	const uint8* NewData = nullptr;
	int32 NewDataSize = 0;

	// read the 2d surface
	FRenderTarget* RenderTarget = GameThread_GetRenderTargetResource();
	if (InTextureFormat == TSF_BGRA8)
	{
		RenderTarget->ReadPixels(NewDataColor);
		check(NewDataColor.Num() == SizeX * SizeY);

		// override the alpha if desired
		if (AlphaOverride)
		{
			check(NewDataColor.Num() == AlphaOverride->Num());
			for (int32 Pixel = 0; Pixel < NewDataColor.Num(); Pixel++)
			{
				NewDataColor[Pixel].A = (*AlphaOverride)[Pixel];
			}
		}
		else if (Flags & CTF_RemapAlphaAsMasked)
		{
			// if the target was rendered with a masked texture, then the depth will probably have been written instead of 0/255 for the
			// alpha, and the depth when unwritten will be 255, so remap 255 to 0 (masked out area) and anything else as 255 (written to area)
			for (int32 Pixel = 0; Pixel < NewDataColor.Num(); Pixel++)
			{
				NewDataColor[Pixel].A = (NewDataColor[Pixel].A == 255) ? 0 : 255;
			}
		}
		else if (Flags & CTF_ForceOpaque)
		{
			for (int32 Pixel = 0; Pixel < NewDataColor.Num(); Pixel++)
			{
				NewDataColor[Pixel].A = 255;
			}
		}

		NewData = (uint8*)NewDataColor.GetData();
		NewDataSize = NewDataColor.Num() * NewDataColor.GetTypeSize();
	}
	else if (InTextureFormat == TSF_RGBA16F)
	{
		RenderTarget->ReadFloat16Pixels(NewDataFloat16Color);
		check(NewDataFloat16Color.Num() == SizeX * SizeY);

		// override the alpha if desired
		if (AlphaOverride)
		{
			check(NewDataFloat16Color.Num() == AlphaOverride->Num());
			for (int32 Pixel = 0; Pixel < NewDataFloat16Color.Num(); Pixel++)
			{
				NewDataFloat16Color[Pixel].A = ((float)(*AlphaOverride)[Pixel]) / 255.0f;
			}
		}
		else if (Flags & CTF_RemapAlphaAsMasked)
		{
			// if the target was rendered with a masked texture, then the depth will probably have been written instead of 0/255 for the
			// alpha, and the depth when unwritten will be 255, so remap 255 to 0 (masked out area) and anything else as 1 (written to area)
			for (int32 Pixel = 0; Pixel < NewDataFloat16Color.Num(); Pixel++)
			{
				NewDataFloat16Color[Pixel].A = (NewDataFloat16Color[Pixel].A == 255) ? 0.0f : 1.0f;
			}
		}
		else if (Flags & CTF_ForceOpaque)
		{
			for (int32 Pixel = 0; Pixel < NewDataFloat16Color.Num(); Pixel++)
			{
				NewDataFloat16Color[Pixel].A = 1.0f;
			}
		}

		NewData = (uint8*)NewDataFloat16Color.GetData();
		NewDataSize = NewDataFloat16Color.Num() * NewDataFloat16Color.GetTypeSize();
	}
	else if (InTextureFormat == TSF_G8)
	{
		RenderTarget->ReadPixels(NewDataColor);
		check(NewDataColor.Num() == SizeX * SizeY);
		NewDataGrayscale.SetNumUninitialized(NewDataColor.Num());

		for (int32 Pixel = 0; Pixel < NewDataColor.Num(); Pixel++)
		{
			NewDataGrayscale[Pixel] = NewDataColor[Pixel].R;
		}

		NewData = (uint8*)NewDataGrayscale.GetData();
		NewDataSize = NewDataGrayscale.Num() * NewDataGrayscale.GetTypeSize();
	}


	if (bHashContent)
	{
		uint32 NewDataHash = FCrc::MemCrc32(NewData, NewDataSize);
		bTextureChanging = OldDataHash != NewDataHash;
	}

	if (bTextureChanging)
	{
		TextureChangingDelegate.ExecuteIfBound(InTexture2D);

		// init to the same size as the 2d texture
		InTexture2D->Source.Init(SizeX, SizeY, NbSlices, NbMips, InTextureFormat, NewData);
		InTexture2D->CompressionSettings = CompressionSettingsForTexture;
	}
#endif
}

/*-----------------------------------------------------------------------------
	FTextureRenderTarget2DResource
-----------------------------------------------------------------------------*/

FTextureRenderTarget2DResource::FTextureRenderTarget2DResource(const class UTextureRenderTarget2D* InOwner)
	:	Owner(InOwner)
	,	ClearColor(InOwner->ClearColor)
	,	Format(InOwner->GetFormat())
	,	TargetSizeX(Owner->SizeX)
	,	TargetSizeY(Owner->SizeY)
{
	
}

/**
 * Clamp size of the render target resource to max values
 *
 * @param MaxSizeX max allowed width
 * @param MaxSizeY max allowed height
 */
void FTextureRenderTarget2DResource::ClampSize(int32 MaxSizeX,int32 MaxSizeY)
{
	// upsize to go back to original or downsize to clamp to max
 	int32 NewSizeX = FMath::Min<int32>(Owner->SizeX,MaxSizeX);
 	int32 NewSizeY = FMath::Min<int32>(Owner->SizeY,MaxSizeY);
	if (NewSizeX != TargetSizeX || NewSizeY != TargetSizeY)
	{
		TargetSizeX = NewSizeX;
		TargetSizeY = NewSizeY;
		// reinit the resource with new TargetSizeX,TargetSizeY
		check(TargetSizeX >= 0 && TargetSizeY >= 0);
		UpdateRHI();
	}	
}

ETextureCreateFlags FTextureRenderTarget2DResource::GetCreateFlags()
{
	// Create the RHI texture. Only one mip is used and the texture is targetable for resolve.
	ETextureCreateFlags TexCreateFlags = Owner->IsSRGB() ? ETextureCreateFlags::SRGB : ETextureCreateFlags::None;
	TexCreateFlags |= Owner->bGPUSharedFlag ? ETextureCreateFlags::Shared : ETextureCreateFlags::None;
	
	if (Owner->bAutoGenerateMips)
	{
		TexCreateFlags |= ETextureCreateFlags::GenerateMipCapable;
		if (FGenerateMips::WillFormatSupportCompute(Format))
		{
			TexCreateFlags |= ETextureCreateFlags::UAV;
		}
	}

	if (Owner->bCanCreateUAV)
	{
		TexCreateFlags |= ETextureCreateFlags::UAV;
	}

	return TexCreateFlags | ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource;
}

/**
 * Initializes the RHI render target resources used by this resource.
 * Called when the resource is initialized, or when reseting all RHI resources.
 * This is only called by the rendering thread.
 */
void FTextureRenderTarget2DResource::InitDynamicRHI()
{
	if( TargetSizeX > 0 && TargetSizeY > 0 )
	{
		FString ResourceName = Owner->GetName();
		ETextureCreateFlags TexCreateFlags = GetCreateFlags();

		FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(*ResourceName)
			.SetExtent(Owner->SizeX, Owner->SizeY)
			.SetFormat(Format)
			.SetNumMips(Owner->GetNumMips())
			.SetFlags(TexCreateFlags)
			.SetInitialState(ERHIAccess::SRVMask)
			.SetClearValue(FClearValueBinding(ClearColor));

		TextureRHI = RenderTargetTextureRHI = RHICreateTexture(Desc);

		if (Owner->bNeedsTwoCopies)
		{
			Desc.SetFlags(TexCreateFlags | ETextureCreateFlags::ShaderResource);

			TextureRHI = RHICreateTexture(Desc);
		}

		if (EnumHasAnyFlags(TexCreateFlags, ETextureCreateFlags::UAV))
		{
			UnorderedAccessViewRHI = RHICreateUnorderedAccessView(RenderTargetTextureRHI);
		}

		SetGPUMask(FRHIGPUMask::All());
		RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, TextureRHI);

		AddToDeferredUpdateList(true);
	}

	// Create the sampler state RHI resource.
	FSamplerStateInitializerRHI SamplerStateInitializer
	(
		(ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter( Owner ),
		Owner->AddressX == TA_Wrap ? AM_Wrap : (Owner->AddressX == TA_Clamp ? AM_Clamp : AM_Mirror),
		Owner->AddressY == TA_Wrap ? AM_Wrap : (Owner->AddressY == TA_Clamp ? AM_Clamp : AM_Mirror),
		AM_Wrap
	);
	SamplerStateRHI = GetOrCreateSamplerState( SamplerStateInitializer );
}

/**
 * Release the RHI render target resources used by this resource.
 * Called when the resource is released, or when reseting all RHI resources.
 * This is only called by the rendering thread.
 */
void FTextureRenderTarget2DResource::ReleaseDynamicRHI()
{
	// release the FTexture RHI resources here as well
	ReleaseRHI();

	RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, nullptr);
	RenderTargetTextureRHI.SafeRelease();
	MipGenerationCache.SafeRelease();

	// remove grom global list of deferred clears
	RemoveFromDeferredUpdateList();
}

#include "SceneUtils.h"
/**
 * Updates (resolves) the render target texture.
 * Optionally clears the contents of the render target to green.
 * This is only called by the rendering thread.
 */
void FTextureRenderTarget2DResource::UpdateDeferredResource( FRHICommandListImmediate& RHICmdList, bool bClearRenderTarget/*=true*/ )
{
	SCOPED_DRAW_EVENT(RHICmdList, GPUResourceUpdate)
	RemoveFromDeferredUpdateList();

	// Skip executing an empty graph.
	if (TextureRHI == RenderTargetTextureRHI && !bClearRenderTarget && !Owner->bAutoGenerateMips)
	{
		return;
	}

	FRDGBuilder GraphBuilder(RHICmdList);

	CacheRenderTarget(RenderTargetTextureRHI, TEXT("MipGeneration"), MipGenerationCache);
	FRDGTextureRef RenderTargetTextureRDG = GraphBuilder.RegisterExternalTexture(MipGenerationCache);
	FRDGTextureRef TextureRDG = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(TextureRHI, TEXT("TextureRenderTarget2DResource")));

 	// clear the target surface to green
	if (bClearRenderTarget)
	{
		ensure(RenderTargetTextureRHI.IsValid() && (RenderTargetTextureRHI->GetClearColor() == ClearColor));
		AddClearRenderTargetPass(GraphBuilder, RenderTargetTextureRDG, ClearColor);
	}
 
	if (Owner->bAutoGenerateMips)
	{
		/**Convert the input values from the editor to a compatible format for FSamplerStateInitializerRHI. 
			Ensure default sampler is Bilinear clamp*/
		FGenerateMips::Execute(GraphBuilder, GetFeatureLevel(), RenderTargetTextureRDG, FGenerateMipsParams{
			Owner->MipsSamplerFilter == TF_Nearest ? SF_Point : (Owner->MipsSamplerFilter == TF_Trilinear ? SF_Trilinear : SF_Bilinear),
			Owner->MipsAddressU == TA_Wrap ? AM_Wrap : (Owner->MipsAddressU == TA_Mirror ? AM_Mirror : AM_Clamp),
			Owner->MipsAddressV == TA_Wrap ? AM_Wrap : (Owner->MipsAddressV == TA_Mirror ? AM_Mirror : AM_Clamp)});
	}

	AddCopyTexturePass(GraphBuilder, RenderTargetTextureRDG, TextureRDG, FRHICopyTextureInfo());

	GraphBuilder.Execute();
}

void FTextureRenderTarget2DResource::Resize(int32 NewSizeX, int32 NewSizeY)
{
	if (TargetSizeX != NewSizeX || TargetSizeY != NewSizeY)
	{
		TargetSizeX = NewSizeX;
		TargetSizeY = NewSizeY;
		UpdateRHI();
	}
}

/** 
 * @return width of target
 */
uint32 FTextureRenderTarget2DResource::GetSizeX() const
{
	return TargetSizeX;
}

/** 
 * @return height of target
 */
uint32 FTextureRenderTarget2DResource::GetSizeY() const
{
	return TargetSizeY;
}

/** 
 * @return dimensions of target surface
 */
FIntPoint FTextureRenderTarget2DResource::GetSizeXY() const
{ 
	return FIntPoint(TargetSizeX, TargetSizeY); 
}

/** 
* Render target resource should be sampled in linear color space
*
* @return display gamma expected for rendering to this render target 
*/
float FTextureRenderTarget2DResource::GetDisplayGamma() const
{
	if (Owner->TargetGamma > UE_KINDA_SMALL_NUMBER * 10.0f)
	{
		return Owner->TargetGamma;
	}
	if (Format == PF_FloatRGB || Format == PF_FloatRGBA || Owner->bForceLinearGamma )
	{
		return 1.0f;
	}
	return FTextureRenderTargetResource::GetDisplayGamma();
}
