// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/AnimBlueprintThumbnailRenderer.h"
#include "ShowFlags.h"
#include "SceneView.h"
#include "Misc/App.h"
#include "Animation/AnimBlueprint.h"

UAnimBlueprintThumbnailRenderer::UAnimBlueprintThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UAnimBlueprintThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Object);
	return AnimBlueprint->BlueprintType != BPTYPE_Interface && !AnimBlueprint->bIsTemplate;
}

void UAnimBlueprintThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Object);
	if (AnimBlueprint && AnimBlueprint->GeneratedClass)
	{
		TSharedRef<FAnimBlueprintThumbnailScene> ThumbnailScene = ThumbnailScenes.EnsureThumbnailScene(AnimBlueprint->GeneratedClass);

		if(ThumbnailScene->SetAnimBlueprint(AnimBlueprint))
		{
			FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
				.SetTime(UThumbnailRenderer::GetTime())
				.SetAdditionalViewFamily(bAdditionalViewFamily));

			ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
			ViewFamily.EngineShowFlags.MotionBlur = 0;
			ViewFamily.EngineShowFlags.LOD = 0;

			RenderViewFamily(Canvas, &ViewFamily, ThumbnailScene->CreateView(&ViewFamily, X, Y, Width, Height));
		}
	}
}

void UAnimBlueprintThumbnailRenderer::BeginDestroy()
{
	ThumbnailScenes.Clear();

	Super::BeginDestroy();
}
