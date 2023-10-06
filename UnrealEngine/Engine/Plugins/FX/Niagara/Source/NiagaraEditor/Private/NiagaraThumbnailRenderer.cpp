// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraThumbnailRenderer.h"
#include "CanvasTypes.h"
#include "Engine/Texture2D.h"
#include "NiagaraEmitter.h"
#include "NiagaraSystem.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraThumbnailRenderer)

bool UNiagaraThumbnailRendererBase::CanVisualizeAsset(UObject* Object)
{
	return GetThumbnailTextureFromObject(Object) != nullptr;
}

void UNiagaraThumbnailRendererBase::GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const
{
	Super::GetThumbnailSize(GetThumbnailTextureFromObject(Object), Zoom, OutWidth, OutHeight);
}

void UNiagaraThumbnailRendererBase::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	Super::Draw(GetThumbnailTextureFromObject(Object), X, Y, Width, Height, RenderTarget, Canvas, bAdditionalViewFamily);
}

UTexture2D* UNiagaraEmitterThumbnailRenderer::GetThumbnailTextureFromObject(UObject* Object) const
{
	UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(Object);
	if (Emitter && Emitter->ThumbnailImage)
	{
		// Avoid calling UpdateResource on cooked texture as doing so will destroy the texture's data
		if (!Emitter->GetPackage()->HasAnyPackageFlags(PKG_Cooked | PKG_FilterEditorOnly))
		{
			Emitter->ThumbnailImage->FinishCachePlatformData();
			Emitter->ThumbnailImage->UpdateResource();
		}
		return Emitter->ThumbnailImage;
	}
	return nullptr;
}

UTexture2D* UNiagaraSystemThumbnailRenderer::GetThumbnailTextureFromObject(UObject* Object) const
{
	UNiagaraSystem* System = Cast<UNiagaraSystem>(Object);
	if (System && System->ThumbnailImage)
	{
		// Avoid calling UpdateResource on cooked texture as doing so will destroy the texture's data
		if (!System->GetPackage()->HasAnyPackageFlags(PKG_Cooked | PKG_FilterEditorOnly))
		{
			System->ThumbnailImage->FinishCachePlatformData();
			System->ThumbnailImage->UpdateResource();
		}
		return System->ThumbnailImage;
	}
	return nullptr;
}
