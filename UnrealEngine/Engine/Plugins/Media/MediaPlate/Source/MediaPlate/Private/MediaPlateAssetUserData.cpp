// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaPlateAssetUserData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MediaPlateAssetUserData)

void UMediaPlateAssetUserData::PostEditChangeOwner()
{
	OnPostEditChangeOwner.ExecuteIfBound();
}
