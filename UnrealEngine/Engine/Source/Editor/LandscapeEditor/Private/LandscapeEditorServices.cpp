// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorServices.h"
#include "SLandscapeLayerListDialog.h"

#if WITH_EDITOR
int32 FLandscapeEditorServices::GetOrCreateEditLayer(FName InEditLayerName, ALandscape* InTargetLandscape)
{
	int32 ExistingLayerIndex = InTargetLandscape->GetLayerIndex(InEditLayerName);
	if (ExistingLayerIndex == INDEX_NONE)
	{
		InTargetLandscape->CreateLayer(InEditLayerName);
		TSharedPtr<SLandscapeLayerListDialog> Dialog = SNew(SLandscapeLayerListDialog, InTargetLandscape->LandscapeLayers);
		Dialog->ShowModal();
		ExistingLayerIndex = Dialog->InsertedLayerIndex;
	}
	return ExistingLayerIndex;
}
#endif // WITH_EDITOR