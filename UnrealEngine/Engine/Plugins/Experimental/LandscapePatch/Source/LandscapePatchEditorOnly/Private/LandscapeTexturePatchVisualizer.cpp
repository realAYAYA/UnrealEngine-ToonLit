// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeTexturePatchVisualizer.h"

#include "Landscape.h"
#include "LandscapeTexturePatch.h"

#include "SceneManagement.h" // FPrimitiveDrawInterface

void FLandscapeTexturePatchVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FColor Color = FColor::Red;
	float Thickness = 3;
	float DepthBias = 1;
	bool bScreenSpace = 1;

	if (const ULandscapeTexturePatch* Patch = Cast<ULandscapeTexturePatch>(Component))
	{
		FTransform PatchToWorld = Patch->GetPatchToWorldTransform();
		DrawRectangle(PDI, PatchToWorld.GetTranslation(), PatchToWorld.GetUnitAxis(EAxis::X), PatchToWorld.GetUnitAxis(EAxis::Y),
			Color, Patch->GetUnscaledCoverage().X * PatchToWorld.GetScale3D().X, Patch->GetUnscaledCoverage().Y * PatchToWorld.GetScale3D().Y, SDPG_Foreground,
			Thickness, DepthBias, bScreenSpace);
	}
	else
	{
		ensure(false);
	}
}