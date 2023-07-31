// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaPlateAssetUserData.h"

void UMediaPlateAssetUserData::PostEditChangeOwner()
{
	OnPostEditChangeOwner.ExecuteIfBound();
}
