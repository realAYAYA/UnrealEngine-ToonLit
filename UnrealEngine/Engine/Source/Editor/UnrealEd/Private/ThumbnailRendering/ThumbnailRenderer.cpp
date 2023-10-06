// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/ThumbnailRenderer.h"

#include "RendererInterface.h"
#include "EngineModule.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "LegacyScreenPercentageDriver.h"

UThumbnailRenderer::UThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// static
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void UThumbnailRenderer::RenderViewFamily(FCanvas* Canvas, FSceneViewFamily* ViewFamily)
{
	if ((ViewFamily == nullptr) || ViewFamily->Views.IsEmpty() || (ViewFamily->Views[0] == nullptr))
	{
		return;
	}

	// The new prototype of RenderViewFamily takes a non-const FSceneView in parameter since view extensions can still modify it as well as as the view family before rendering : 
	FSceneView* View = const_cast<FSceneView*>(ViewFamily->Views[0]);
	UThumbnailRenderer::RenderViewFamily(Canvas, ViewFamily, View);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

// static
void UThumbnailRenderer::RenderViewFamily(FCanvas* Canvas, FSceneViewFamily* ViewFamily, FSceneView* View)
{
	if ((ViewFamily == nullptr) || (View == nullptr))
	{ 
		return;
	}

	check(ViewFamily->Views.Num() == 1 && (ViewFamily->Views[0] == View));

	ViewFamily->EngineShowFlags.ScreenPercentage = false;
	ViewFamily->SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(
		*ViewFamily, /* GlobalResolutionFraction = */ 1.0f));

	ViewFamily->ViewExtensions = GEngine->ViewExtensions->GatherActiveExtensions(FSceneViewExtensionContext(ViewFamily->Scene));

	// View extensions should have a chance at changing ViewFamily and View (while it's still mutable, since ViewFamily->Views contains const FSceneView pointers) before rendering :
	for (const FSceneViewExtensionRef& Extension : ViewFamily->ViewExtensions)
	{
		Extension->SetupViewFamily(*ViewFamily);
		Extension->SetupView(*ViewFamily, *View);
	}

	GetRendererModule().BeginRenderingViewFamily(Canvas, ViewFamily);
}

// static
FGameTime UThumbnailRenderer::GetTime()
{
	return FGameTime::GetTimeSinceAppStart();
}
