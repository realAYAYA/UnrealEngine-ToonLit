// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamBlueprintAssetUserData.h"

#include "VCamComponent.h"

#if WITH_EDITOR
void UVCamBlueprintAssetUserData::PostEditUndo()
{
	GetOuterUVCamComponent()->OnAssetUserDataPostEditUndo();
}
#endif