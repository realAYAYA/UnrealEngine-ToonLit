// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeVirtualTextureThumbnailRenderer.h"

#include "Components/RuntimeVirtualTextureComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialRenderProxy.h"
#include "MaterialShared.h"
#include "Math/Box.h"
#include "Math/Box2D.h"
#include "Math/BoxSphereBounds.h"
#include "Math/Transform.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "PixelFormat.h"
#include "RHI.h"
#include "RenderGraphBuilder.h"
#include "RenderingThread.h"
#include "SceneInterface.h"
#include "Templates/Casts.h"
#include "UObject/Object.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "VT/RuntimeVirtualTexture.h"
#include "VT/RuntimeVirtualTextureEnum.h"
#include "VT/RuntimeVirtualTextureRender.h"
#include "VirtualTexturing.h"

class FCanvas;
class FRHICommandListImmediate;

namespace
{
	/** Find a matching component for this URuntimeVirtualTexture. */
	URuntimeVirtualTextureComponent* FindComponent(URuntimeVirtualTexture* RuntimeVirtualTexture)
	{
		for (TObjectIterator<URuntimeVirtualTextureComponent> It; It; ++It)
		{
			URuntimeVirtualTextureComponent* RuntimeVirtualTextureComponent = *It;
			if (RuntimeVirtualTextureComponent->GetVirtualTexture() == RuntimeVirtualTexture)
			{
				return RuntimeVirtualTextureComponent;
			}
		}

		return nullptr;
	}
}

URuntimeVirtualTextureThumbnailRenderer::URuntimeVirtualTextureThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool URuntimeVirtualTextureThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	URuntimeVirtualTexture* RuntimeVirtualTexture = Cast<URuntimeVirtualTexture>(Object);

	// We need a matching URuntimeVirtualTextureComponent in a Scene to be able to render a thumbnail
	URuntimeVirtualTextureComponent* RuntimeVirtualTextureComponent = FindComponent(RuntimeVirtualTexture);
	if (RuntimeVirtualTextureComponent != nullptr && RuntimeVirtualTextureComponent->GetScene() != nullptr)
	{
		return true;
	}

	return false;
}

void URuntimeVirtualTextureThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	//todo[vt]: Handle case where a null or floating point render target is passed in. (This happens on package save.)
	if (RenderTarget->GetRenderTargetTexture() == nullptr || RenderTarget->GetRenderTargetTexture()->GetFormat() != PF_B8G8R8A8)
	{
		return;
	}

	URuntimeVirtualTexture* RuntimeVirtualTexture = Cast<URuntimeVirtualTexture>(Object);
	URuntimeVirtualTextureComponent* RuntimeVirtualTextureComponent = FindComponent(RuntimeVirtualTexture);
	FSceneInterface* Scene = RuntimeVirtualTextureComponent != nullptr ? RuntimeVirtualTextureComponent->GetScene() : nullptr;
	check(Scene != nullptr);

	if (UWorld* World = Scene->GetWorld())
	{
		World->SendAllEndOfFrameUpdates();
	}

	const FBox2D DestBox = FBox2D(FVector2D(X, Y), FVector2D(Width, Height));
	const FTransform Transform = RuntimeVirtualTextureComponent->GetComponentTransform();
	const FBox Bounds = RuntimeVirtualTextureComponent->Bounds.GetBox();
	const uint32 VirtualTextureSceneIndex = RuntimeVirtualTexture::GetRuntimeVirtualTextureSceneIndex_GameThread(RuntimeVirtualTextureComponent);
	const ERuntimeVirtualTextureMaterialType MaterialType = RuntimeVirtualTexture->GetMaterialType();

	FVTProducerDescription VTDesc;
	RuntimeVirtualTexture->GetProducerDescription(VTDesc, URuntimeVirtualTexture::FInitSettings(), Transform);
	const int32 MaxLevel = (int32)FMath::CeilLogTwo(FMath::Max(VTDesc.BlockWidthInTiles, VTDesc.BlockHeightInTiles));

	UE::RenderCommandPipe::FSyncScope SyncScope;

	ENQUEUE_RENDER_COMMAND(BakeStreamingTextureTileCommand)(
		[Scene, VirtualTextureSceneIndex, MaterialType, RenderTarget, DestBox, Transform, Bounds, MaxLevel](FRHICommandListImmediate& RHICmdList)
	{
		FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();

		FRDGBuilder GraphBuilder(RHICmdList);

		RuntimeVirtualTexture::FRenderPageBatchDesc Desc;
		Desc.Scene = Scene->GetRenderScene();
		Desc.RuntimeVirtualTextureMask = 1 << VirtualTextureSceneIndex;
		Desc.UVToWorld = Transform;
		Desc.WorldBounds = Bounds;
		Desc.MaterialType = MaterialType;
		Desc.MaxLevel = MaxLevel;
		Desc.bClearTextures = true;
		Desc.bIsThumbnails = true;
		Desc.FixedColor = FLinearColor::Transparent;
		Desc.NumPageDescs = 1;
		Desc.Targets[0].Texture = RenderTarget->GetRenderTargetTexture();
		Desc.PageDescs[0].DestBox[0] = DestBox;
		Desc.PageDescs[0].UVRange = FBox2D(FVector2D(0, 0), FVector2D(1, 1));
		Desc.PageDescs[0].vLevel = MaxLevel;

		RuntimeVirtualTexture::RenderPagesStandAlone(GraphBuilder, Desc);

		GraphBuilder.Execute();
	});
}
