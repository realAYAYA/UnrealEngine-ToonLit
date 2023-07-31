// Copyright Epic Games, Inc. All Rights Reserved.

#include "VT/RuntimeVirtualTextureSceneProxy.h"

#include "Components/RuntimeVirtualTextureComponent.h"
#include "RendererOnScreenNotification.h"
#include "VirtualTextureSystem.h"
#include "VT/RuntimeVirtualTexture.h"
#include "VT/RuntimeVirtualTextureProducer.h"
#include "VT/VirtualTexture.h"
#include "VT/VirtualTextureBuilder.h"
#include "VT/VirtualTextureScalability.h"

#define LOCTEXT_NAMESPACE "VirtualTexture"

int32 FRuntimeVirtualTextureSceneProxy::ProducerIdGenerator = 1;

FRuntimeVirtualTextureSceneProxy::FRuntimeVirtualTextureSceneProxy(URuntimeVirtualTextureComponent* InComponent)
	: SceneIndex(0)
	, ProducerId(0)
	, VirtualTexture(nullptr)
	, bHidePrimitivesInEditor(false)
	, bHidePrimitivesInGame(false)
	, CombinedDirtyRect(0, 0, 0, 0)
{
	// Evaluate the flags used to hide primitives writing to this virtual texture.
	InComponent->GetHidePrimitiveSettings(bHidePrimitivesInEditor, bHidePrimitivesInGame);

	if (InComponent->GetVirtualTexture() != nullptr)
	{
		// We store a ProducerId here so that we will be able to find our SceneIndex from the Producer during rendering.
		// We will need the SceneIndex to determine which primitives should render to this Producer.
		ProducerId = ProducerIdGenerator++;

		URuntimeVirtualTexture::FInitSettings InitSettings;
		InitSettings.TileCountBias = InComponent->IsScalable() ? VirtualTextureScalability::GetRuntimeVirtualTextureSizeBias(InComponent->GetScalabilityGroup()) : 0;

		VirtualTexture = InComponent->GetVirtualTexture();
		Transform = InComponent->GetComponentTransform();
		const FBox Bounds = InComponent->Bounds.GetBox();

		// The producer description is calculated using the transform to determine the aspect ratio
		FVTProducerDescription ProducerDesc;
		VirtualTexture->GetProducerDescription(ProducerDesc, InitSettings, Transform);
		VirtualTextureSize = FIntPoint(ProducerDesc.BlockWidthInTiles * ProducerDesc.TileSize, ProducerDesc.BlockHeightInTiles * ProducerDesc.TileSize);
		// We only need to dirty flush up to the producer description MaxLevel which accounts for the RemoveLowMips
		MaxDirtyLevel = ProducerDesc.MaxLevel;

		const ERuntimeVirtualTextureMaterialType MaterialType = VirtualTexture->GetMaterialType();
		const bool bClearTextures = VirtualTexture->GetClearTextures();

		// The producer object created here will be passed into the virtual texture system which will take ownership.
		IVirtualTexture* Producer = new FRuntimeVirtualTextureProducer(ProducerDesc, ProducerId, MaterialType, bClearTextures, InComponent->GetScene(), Transform, Bounds);

		// Create a producer for the streaming low mips. 
		// This is bound with the main producer so that one allocated VT can use both runtime or streaming producers dependent on mip level.
		if (InComponent->IsStreamingLowMips())
		{
			if (InComponent->IsStreamingTextureInvalid())
			{
#if !UE_BUILD_SHIPPING
				// Notify that streaming texture is invalid since this can cause performance regression.
				const FString Name = InComponent->GetPathName();
				OnScreenWarningDelegateHandle = FRendererOnScreenNotification::Get().AddLambda([Name](FCoreDelegates::FSeverityMessageMap& OutMessages)
				{
					OutMessages.Add(
						FCoreDelegates::EOnScreenMessageSeverity::Warning,
						FText::Format(LOCTEXT("SVTInvalid", "Runtime Virtual Texture '{0}' streaming mips needs to be rebuilt."), FText::FromString(Name)));
				});
#endif
			}
			else
			{
				UVirtualTexture2D* StreamingTexture = InComponent->GetStreamingTexture()->Texture;

				FVTProducerDescription StreamingProducerDesc;
				IVirtualTexture* StreamingProducer = RuntimeVirtualTexture::CreateStreamingTextureProducer(StreamingTexture, ProducerDesc, StreamingProducerDesc);

				ensure(ProducerDesc.MaxLevel >= StreamingProducerDesc.MaxLevel);
				const int32 TransitionLevel = ProducerDesc.MaxLevel - StreamingProducerDesc.MaxLevel;

				Producer = RuntimeVirtualTexture::BindStreamingTextureProducer(Producer, StreamingProducer, TransitionLevel);

				// Any dirty flushes don't need to flush the streaming mips (they only change with a build step).
				MaxDirtyLevel = TransitionLevel - 1;
			}
		}

		// The Initialize() call will allocate the virtual texture by spawning work on the render thread.
		VirtualTexture->Initialize(Producer, ProducerDesc, Transform, Bounds);

		// Store the ProducerHandle and SpaceID immediately after virtual texture is initialized.
		ENQUEUE_RENDER_COMMAND(GetProducerHandle)(
			[this, VirtualTexturePtr = VirtualTexture](FRHICommandList& RHICmdList)
			{
				ProducerHandle = VirtualTexturePtr->GetProducerHandle();
				SpaceID = VirtualTexturePtr->GetAllocatedVirtualTexture()->GetSpaceID();
			});
	}
}

FRuntimeVirtualTextureSceneProxy::~FRuntimeVirtualTextureSceneProxy()
{
	checkSlow(IsInRenderingThread());

#if !UE_BUILD_SHIPPING
	FRendererOnScreenNotification::Get().Remove(OnScreenWarningDelegateHandle);
#endif
}

void FRuntimeVirtualTextureSceneProxy::Release()
{
	checkSlow(!IsInRenderingThread());

	if (VirtualTexture != nullptr)
	{
		VirtualTexture->Release();
		VirtualTexture = nullptr;
	}
}

void FRuntimeVirtualTextureSceneProxy::Dirty(FBoxSphereBounds const& Bounds)
{
	// Transform world bounds into Virtual Texture UV space
	const FVector O = Transform.GetTranslation();
	const FVector U = Transform.GetUnitAxis(EAxis::X) * 1.f / Transform.GetScale3D().X;
	const FVector V = Transform.GetUnitAxis(EAxis::Y) * 1.f / Transform.GetScale3D().Y;
	const FVector P = Bounds.GetSphere().Center - O;
	const FVector2D UVCenter = FVector2D(FVector::DotProduct(P, U), FVector::DotProduct(P, V));
	const float Scale = FMath::Max(1.f / Transform.GetScale3D().X, 1.f / Transform.GetScale3D().Y);
	const float UVRadius = Bounds.GetSphere().W * Scale;
	const FVector2D UVExtent(UVRadius, UVRadius);
	const FBox2D UVRect = FBox2D(UVCenter - UVExtent, UVCenter + UVExtent);

	// Convert to Texel coordinate space
	const FIntRect TextureRect(0, 0, VirtualTextureSize.X, VirtualTextureSize.Y);
	FIntRect TexelRect(
		FMath::FloorToInt(UVRect.Min.X * VirtualTextureSize.X),
		FMath::FloorToInt(UVRect.Min.Y * VirtualTextureSize.Y),
		FMath::CeilToInt(UVRect.Max.X * VirtualTextureSize.X),
		FMath::CeilToInt(UVRect.Max.Y * VirtualTextureSize.Y));
	TexelRect.Clip(TextureRect);

	// Only add rect if it has some area
	if (TexelRect.Min != TexelRect.Max)
	{
		const bool bFirst = DirtyRects.Add(TexelRect) == 0;
		if (bFirst)
		{
			CombinedDirtyRect = TexelRect;
		}
		else
		{
			CombinedDirtyRect.Union(TexelRect);
		}
	}
}

void FRuntimeVirtualTextureSceneProxy::FlushDirtyPages()
{
	// If Producer handle is not initialized yet it's safe to do nothing because we won't have rendered anything to the VT that needs flushing.
	if (ProducerHandle.PackedValue != 0)
	{
		// Don't do any work if we won't mark anything dirty.
		if (MaxDirtyLevel >= 0 && CombinedDirtyRect.Width() != 0 && CombinedDirtyRect.Height() != 0)
		{
			//todo[vt]: 
			// Profile to work out best heuristic for when we should use the CombinedDirtyRect
			// Also consider using some other structure to represent dirty area such as a course 2D bitfield
			const bool bCombinedFlush = (DirtyRects.Num() > 2 || CombinedDirtyRect == FIntRect(0, 0, VirtualTextureSize.X, VirtualTextureSize.Y));

			if (bCombinedFlush)
			{
				FVirtualTextureSystem::Get().FlushCache(ProducerHandle, SpaceID, CombinedDirtyRect, MaxDirtyLevel);
			}
			else
			{
				for (FIntRect Rect : DirtyRects)
				{
					FVirtualTextureSystem::Get().FlushCache(ProducerHandle, SpaceID, Rect, MaxDirtyLevel);
				}
			}
		}
	}

	DirtyRects.Reset();
	CombinedDirtyRect = FIntRect(0, 0, 0, 0);
}

#undef LOCTEXT_NAMESPACE
