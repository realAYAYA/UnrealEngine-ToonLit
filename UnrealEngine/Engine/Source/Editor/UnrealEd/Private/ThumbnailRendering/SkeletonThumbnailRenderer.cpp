// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/SkeletonThumbnailRenderer.h"

#include "AssetToolsModule.h"
#include "Misc/App.h"
#include "ShowFlags.h"
#include "SceneView.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "ThumbnailHelpers.h"

USkeletonThumbnailRenderer::USkeletonThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USkeletonThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	USkeleton* Skeleton = Cast<USkeleton>(Object);

	constexpr bool bFindIfNotSet = true;
	if (USkeletalMesh* SkeletalMesh = Skeleton->GetPreviewMesh(bFindIfNotSet))
	{
		if (SkeletalMesh && (SkeletalMesh->IsCompiling() || SkeletalMesh->GetResourceForRendering() == nullptr))
		{
			return false;
		}
	}

	return true;
}

void USkeletonThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	USkeleton* Skeleton = Cast<USkeleton>(Object);
	TSharedRef<FSkeletalMeshThumbnailScene> ThumbnailScene = ThumbnailSceneCache.EnsureThumbnailScene(Object);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	const TSharedPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(USkeleton::StaticClass()).Pin();
	ThumbnailScene->SetDrawDebugSkeleton(true, AssetTypeActions->GetTypeColor());
	
	if(Skeleton)
	{
		constexpr bool bFindIfNotSet = true; 
		if(USkeletalMesh* SkeletalMesh = Skeleton->GetPreviewMesh(bFindIfNotSet))
		{
			ThumbnailScene->SetSkeletalMesh(SkeletalMesh);
		}
	}
	AddAdditionalPreviewSceneContent(Object, ThumbnailScene->GetWorld());

	FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game) )
		.SetTime(UThumbnailRenderer::GetTime())
		.SetAdditionalViewFamily(bAdditionalViewFamily));

	ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
	ViewFamily.EngineShowFlags.MotionBlur = 0;
	ViewFamily.EngineShowFlags.LOD = 0;

	RenderViewFamily(Canvas, &ViewFamily, ThumbnailScene->CreateView(&ViewFamily, X, Y, Width, Height));
	ThumbnailScene->SetSkeletalMesh(nullptr);
}

EThumbnailRenderFrequency USkeletonThumbnailRenderer::GetThumbnailRenderFrequency(UObject* Object) const
{
	USkeleton* Skeleton = Cast<USkeleton>(Object);
	if(Skeleton)
	{
		if(USkeletalMesh* SkeletalMesh = Skeleton->GetPreviewMesh())
		{
			return SkeletalMesh->GetResourceForRendering() != nullptr 
				? EThumbnailRenderFrequency::Realtime
				: EThumbnailRenderFrequency::OnPropertyChange;
		}
	}
	return EThumbnailRenderFrequency::OnPropertyChange;
}

void USkeletonThumbnailRenderer::BeginDestroy()
{
	ThumbnailSceneCache.Clear();

	Super::BeginDestroy();
}
