// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeTextureBackedRenderTarget.h"

#include "LandscapeDataAccess.h"
#include "LandscapePatchUtil.h" // CopyTextureOnRenderThread
#include "LandscapeTexturePatchPS.h"
#include "RenderGraph.h"
#include "TextureCompiler.h"
#include "UObject/ObjectSaveContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandscapeTextureBackedRenderTarget)

namespace LandscapeTextureBackedRenderTargetLocals
{
#if WITH_EDITOR
	UTexture2D* CreateTexture(UObject* Parent)
	{
		UTexture2D* Texture = Parent ? NewObject<UTexture2D>(Parent) : NewObject<UTexture2D>();
		Texture->SRGB = false;
		Texture->MipGenSettings = TMGS_NoMipmaps;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		// TODO: How do we allow lossless compression, instead of disallowing compression entirely? Just setting 
		// LossyCompressionAmount to ETextureLossyCompressionAmount::TLCA_None is not sufficient.
		Texture->CompressionNone = true;

		return Texture;
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void ULandscapeTextureBackedRenderTargetBase::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);

	if (!bUseInternalTextureOnly)
	{
		CopyToInternalTexture();
	}
}

// This gets called not just when loading, but also after duplication.
void ULandscapeTextureBackedRenderTargetBase::PostLoad()
{
	Super::PostLoad();

	if (!bUseInternalTextureOnly && IsValid(InternalTexture))
	{
		InternalTexture->ConditionalPostLoad();

		RenderTarget = NewObject<UTextureRenderTarget2D>(this);
		RenderTarget->RenderTargetFormat = GetRenderTargetFormat();
		RenderTarget->InitAutoFormat(SizeX, SizeY);
		RenderTarget->UpdateResourceImmediate(false);

		CopyBackFromInternalTexture();
	}
}

// PreDuplicate is called not just when objects are copied in editor, but also when they are serialized for undo/redo.
void ULandscapeTextureBackedRenderTargetBase::PreDuplicate(FObjectDuplicationParameters& DupParams)
{
	Super::PreDuplicate(DupParams);

	if (!bUseInternalTextureOnly)
	{
		CopyToInternalTexture();
	}
}

// Called when serializing to text for copy/paste
void ULandscapeTextureBackedRenderTargetBase::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
	Super::ExportCustomProperties(Out, Indent);

	if (!bUseInternalTextureOnly)
	{
		CopyToInternalTexture();
	}
}

// Called after pasting
void ULandscapeTextureBackedRenderTargetBase::PostEditImport()
{
	Super::PostEditImport();

	if (!bUseInternalTextureOnly && IsValid(InternalTexture))
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this);
		RenderTarget->RenderTargetFormat = GetRenderTargetFormat();
		RenderTarget->InitAutoFormat(SizeX, SizeY);
		RenderTarget->UpdateResourceImmediate(false);

		CopyBackFromInternalTexture();
	}
}
#endif // WITH_EDITOR

void ULandscapeTextureBackedRenderTargetBase::SetUseInternalTextureOnly(bool bUseInternalTextureOnlyIn, bool bCopyExisting)
{
#if WITH_EDITOR
	if (bUseInternalTextureOnly == bUseInternalTextureOnlyIn)
	{
		return;
	}

	Modify();
	bUseInternalTextureOnly = bUseInternalTextureOnlyIn;

	if (bUseInternalTextureOnly)
	{
		// We're no longer using the render target, so copy it to internal texture
		if (IsValid(RenderTarget) && bCopyExisting)
		{
			Modify();
			CopyToInternalTexture();
		}
		RenderTarget = nullptr;
	}
	else
	{
		// We're back to using the render target, so initialize it from internal texture.

		if (IsValid(InternalTexture)) // if initialized
		{
			RenderTarget = NewObject<UTextureRenderTarget2D>(this);
			RenderTarget->RenderTargetFormat = GetRenderTargetFormat();
			RenderTarget->InitAutoFormat(SizeX, SizeY);
			RenderTarget->UpdateResourceImmediate(false);

			if (bCopyExisting)
			{
				CopyBackFromInternalTexture();
			}
		}
	}
#endif // WITH_EDITOR
}

void ULandscapeTextureBackedRenderTargetBase::SetResolution(int32 SizeXIn, int32 SizeYIn)
{
#if WITH_EDITOR
	if (SizeXIn == SizeX && SizeYIn == SizeY)
	{
		return;
	}

	Modify();
	SizeX = SizeXIn;
	SizeY = SizeYIn;
	if (IsValid(RenderTarget))
	{
		RenderTarget->Modify();
		RenderTarget->InitAutoFormat(SizeX, SizeY);
		RenderTarget->UpdateResourceImmediate();
	}
	if (IsValid(InternalTexture))
	{
		InternalTexture->Modify();
		InternalTexture->Source.Init(SizeX, SizeY, 1, 1, GetInternalTextureFormat());
		InternalTexture->UpdateResource();
	}
#endif // WITH_EDITOR
}

void ULandscapeTextureBackedRenderTargetBase::Initialize()
{
#if WITH_EDITOR
	using namespace LandscapeTextureBackedRenderTargetLocals;

	if (!IsValid(InternalTexture) || !InternalTexture->GetResource())
	{
		InternalTexture = CreateTexture(this);
	}

	InternalTexture->Source.Init(SizeX, SizeY, 1, 1, GetInternalTextureFormat());
	InternalTexture->UpdateResource();

	if (bUseInternalTextureOnly)
	{
		RenderTarget = nullptr;
	}
	else
	{
		if (!IsValid(RenderTarget))
		{
			RenderTarget = NewObject<UTextureRenderTarget2D>(this);
		}

		RenderTarget->RenderTargetFormat = GetRenderTargetFormat();
		RenderTarget->InitAutoFormat(SizeX, SizeY);
		RenderTarget->UpdateResourceImmediate();
	}
#endif // WITH_EDITOR
}



void ULandscapeWeightTextureBackedRenderTarget::SetUseAlphaChannel(bool bUseAlphaChannelIn)
{
#if WITH_EDITOR
	if (bUseAlphaChannel == bUseAlphaChannelIn)
	{
		return;
	}
	Modify();
	bUseAlphaChannel = bUseAlphaChannelIn;

	// TODO: We could try to copy the non-alpha channel data across, but that is messy with
	// an unclear amount of benefit. It would seem odd for a user to want to write one
	// way but then discard/add the alpha channel afterward. Still, could revisit later.

	if (IsValid(RenderTarget))
	{
		RenderTarget->Modify();
		RenderTarget->RenderTargetFormat = GetRenderTargetFormat();
		RenderTarget->InitAutoFormat(SizeX, SizeY);
		RenderTarget->UpdateResourceImmediate();
	}
	if (IsValid(InternalTexture))
	{
		InternalTexture->Modify();
		InternalTexture->Source.Init(SizeX, SizeY, 1, 1, GetInternalTextureFormat());
		InternalTexture->UpdateResource();
	}
#endif // WITH_EDITOR
}

ETextureRenderTargetFormat ULandscapeWeightTextureBackedRenderTarget::GetRenderTargetFormat()
{
	return bUseAlphaChannel ? ETextureRenderTargetFormat::RTF_RGBA8 : ETextureRenderTargetFormat::RTF_R8;
}

ETextureSourceFormat ULandscapeWeightTextureBackedRenderTarget::GetInternalTextureFormat()
{
	return bUseAlphaChannel ? ETextureSourceFormat::TSF_BGRA8 : ETextureSourceFormat::TSF_G8;
}

void ULandscapeWeightTextureBackedRenderTarget::CopyToInternalTexture()
{
#if WITH_EDITOR
	using namespace LandscapeTextureBackedRenderTargetLocals;

	if (!IsCopyingBackAndForthAllowed())
	{
		return;
	}

	if (!ensure(IsValid(RenderTarget)))
	{
		return;
	}

	if (!(ensure(IsValid(InternalTexture)) && InternalTexture->GetResource()))
	{
		Modify();
		InternalTexture = CreateTexture(this);
		// The sizing and format doesn't matter because the UpdateTexture2D call below will deal with it.
	}
	else
	{
		InternalTexture->Modify();
	}

	RenderTarget->UpdateTexture2D(InternalTexture, GetInternalTextureFormat());
	InternalTexture->UpdateResource();
#endif // WITH_EDITOR
}

void ULandscapeWeightTextureBackedRenderTarget::CopyBackFromInternalTexture()
{
#if WITH_EDITOR
	using namespace LandscapeTextureBackedRenderTargetLocals;

	if (!IsCopyingBackAndForthAllowed())
	{
		return;
	}

	if (!ensure(IsValid(InternalTexture)))
	{
		return;
	}
	InternalTexture->UpdateResource();
	FTextureCompilingManager::Get().FinishCompilation({ InternalTexture });

	if (!InternalTexture->GetResource())
	{
		return;
	}

	bool bCreatedNewRenderTarget = false;
	if (!ensure(IsValid(RenderTarget)))
	{
		Modify();
		RenderTarget = NewObject<UTextureRenderTarget2D>(this);
		bCreatedNewRenderTarget = true;
	}

	if (!ensure(InternalTexture->GetResource()->GetSizeX() == RenderTarget->SizeX
		&& InternalTexture->GetResource()->GetSizeY() == RenderTarget->SizeY
		&& RenderTarget->RenderTargetFormat == GetRenderTargetFormat()))
	{
		if (!bCreatedNewRenderTarget)
		{
			RenderTarget->Modify();
		}
		RenderTarget->RenderTargetFormat = GetRenderTargetFormat();
		RenderTarget->InitAutoFormat(InternalTexture->GetResource()->GetSizeX(), InternalTexture->GetResource()->GetSizeY());
		RenderTarget->UpdateResourceImmediate(false);
	}

	FTextureResource* Source = InternalTexture->GetResource();
	FTextureResource* Destination = RenderTarget->GetResource();

	ENQUEUE_RENDER_COMMAND(LandscapeTextureHeightPatchRTToTexture)(
		[Source, Destination](FRHICommandListImmediate& RHICmdList)
		{
			UE::Landscape::PatchUtil::CopyTextureOnRenderThread(RHICmdList, *Source, *Destination);
		});
#endif // WITH_EDITOR
}


void ULandscapeHeightTextureBackedRenderTarget::SetFormat(ETextureRenderTargetFormat FormatToUse)
{
#if WITH_EDITOR
	if (RenderTargetFormat == FormatToUse)
	{
		return;
	}
	Modify();
	RenderTargetFormat = FormatToUse;

	// We could try to copy over existing data, but that is not worth it.

	if (IsValid(RenderTarget))
	{
		RenderTarget->Modify();
		RenderTarget->RenderTargetFormat = GetRenderTargetFormat();
		RenderTarget->InitAutoFormat(SizeX, SizeY);
		RenderTarget->UpdateResourceImmediate();
	}
#endif // WITH_EDITOR
}

void ULandscapeHeightTextureBackedRenderTarget::CopyToInternalTexture()
{
#if WITH_EDITOR
	using namespace LandscapeTextureBackedRenderTargetLocals;

	if (!IsCopyingBackAndForthAllowed())
	{
		return;
	}

	if (!ensure(IsValid(RenderTarget)))
	{
		return;
	}

	if (!(ensure(IsValid(InternalTexture)) && InternalTexture->GetResource()))
	{
		Modify();
		InternalTexture = CreateTexture(this);
	}
	else
	{
		InternalTexture->Modify();
	}

	UTextureRenderTarget2D* NativeEncodingRenderTarget = RenderTarget;

	// If the format doesn't match the format that we use generally for our internal texture, save the patch in our native
	// height format, applying whatever scale/offset is relevant. The stored texture thus ends up being the native equivalent
	// (with scale 1 and offset 0). This is easier than trying to support various kinds of RT-to-texture conversions.
	if (NativeEncodingRenderTarget->RenderTargetFormat != ETextureRenderTargetFormat::RTF_RGBA8)
	{
		// We need a temporary render target to write the converted result, then we'll copy that to the texture.
		NativeEncodingRenderTarget = NewObject<UTextureRenderTarget2D>(this);
		NativeEncodingRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
		NativeEncodingRenderTarget->InitAutoFormat(RenderTarget->SizeX, RenderTarget->SizeY);
		NativeEncodingRenderTarget->UpdateResourceImmediate(false);

		FTextureResource* Source = RenderTarget->GetResource();
		FTextureResource* Destination = NativeEncodingRenderTarget->GetResource();

		ENQUEUE_RENDER_COMMAND(LandscapeTextureHeightPatchRTToTexture)(
			[this, Source, Destination](FRHICommandListImmediate& RHICmdList)
			{
				using namespace UE::Landscape;

				FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("LandscapeTextureHeightPatchConvertToNative"));

				FRDGTextureRef SourceTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Source->GetTexture2DRHI(), TEXT("ConversionSource")));
				FRDGTextureRef DestinationTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Destination->GetTexture2DRHI(), TEXT("ConversionDestination")));

				FConvertToNativeLandscapePatchPS::AddToRenderGraph(GraphBuilder, SourceTexture, DestinationTexture, ConversionParams);

				GraphBuilder.Execute();
			});
	}

	// TODO: The header for this requires the texture to be square power of 2, but it actually doesn't seem to
	// be an enforced requirement. If that changes, we'll need our own ReadPixels followed by locking a mip
	// and writing to it.
	// This call does a flush for us, so the render target should be updated.
	NativeEncodingRenderTarget->UpdateTexture2D(InternalTexture, GetInternalTextureFormat());
	InternalTexture->UpdateResource();
#endif // WITH_EDITOR
}

void ULandscapeHeightTextureBackedRenderTarget::CopyBackFromInternalTexture()
{
#if WITH_EDITOR
	using namespace LandscapeTextureBackedRenderTargetLocals;

	if (!IsCopyingBackAndForthAllowed())
	{
		return;
	}

	if (!ensure(IsValid(InternalTexture)))
	{
		return;
	}

	InternalTexture->UpdateResource();
	FTextureCompilingManager::Get().FinishCompilation({ InternalTexture });

	if (!InternalTexture->GetResource())
	{
		return;
	}

	bool bCreatedNewRenderTarget = false;
	if (!ensure(IsValid(RenderTarget)))
	{
		Modify();
		RenderTarget = NewObject<UTextureRenderTarget2D>(this);
		bCreatedNewRenderTarget = true;
	}

	if (!ensure(InternalTexture->GetResource()->GetSizeX() == RenderTarget->SizeX
		&& InternalTexture->GetResource()->GetSizeY() == RenderTarget->SizeY
		&& RenderTarget->RenderTargetFormat == GetRenderTargetFormat()))
	{
		if (!bCreatedNewRenderTarget)
		{
			RenderTarget->Modify();
		}
		RenderTarget->RenderTargetFormat = GetRenderTargetFormat();
		RenderTarget->InitAutoFormat(InternalTexture->GetResource()->GetSizeX(), InternalTexture->GetResource()->GetSizeY());
	}

	FTextureResource* Source = InternalTexture->GetResource();
	FTextureResource* Destination = RenderTarget->GetResource();
	if (!ensure(Source && Destination))
	{
		return;
	}

	// If we're in a different format, we need to "un-bake" the height from the texture.
	if (RenderTarget->RenderTargetFormat != ETextureRenderTargetFormat::RTF_RGBA8)
	{
		ENQUEUE_RENDER_COMMAND(LandscapeTextureHeightPatchRTToTexture)(
			[this, Source, Destination](FRHICommandListImmediate& RHICmdList)
			{
				using namespace UE::Landscape;

				FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("LandscapeTextureHeightPatchConvertFromNative"));

				FRDGTextureRef SourceTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Source->GetTexture2DRHI(), TEXT("ConversionSource")));
				FRDGTextureRef DestinationTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Destination->GetTexture2DRHI(), TEXT("ConversionDestination")));

				FConvertBackFromNativeLandscapePatchPS::AddToRenderGraph(GraphBuilder, SourceTexture, DestinationTexture, ConversionParams);

				GraphBuilder.Execute();
			});
	}
	else
	{
		// When formats match, we can just copy back and forth.
		ENQUEUE_RENDER_COMMAND(LandscapeTextureHeightPatchRTToTexture)(
			[Source, Destination](FRHICommandListImmediate& RHICmdList)
			{
				UE::Landscape::PatchUtil::CopyTextureOnRenderThread(RHICmdList, *Source, *Destination);
			});
	}
#endif // WITH_EDITOR
}

bool ULandscapeTextureBackedRenderTargetBase::IsCopyingBackAndForthAllowed()
{
	UWorld* World = GetWorld();
	return IsValid(this) && !IsTemplate()
		// Note that having a null world is ok because we get that temporarily while rerunning construction
		// scripts. However if we do have a world, it should be the normal editor one.
		&& (!World || (IsValid(World) && World->WorldType == EWorldType::Editor))
		&& FApp::CanEverRender();
}
