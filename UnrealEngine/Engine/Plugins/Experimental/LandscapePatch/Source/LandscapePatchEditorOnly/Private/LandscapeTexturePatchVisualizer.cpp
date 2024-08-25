// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeTexturePatchVisualizer.h"

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
	// Note: when patches are deleted, selection seems to be updated only after giving another call to the
	// visualization drawing, so do not ensure if patch was null.
}
