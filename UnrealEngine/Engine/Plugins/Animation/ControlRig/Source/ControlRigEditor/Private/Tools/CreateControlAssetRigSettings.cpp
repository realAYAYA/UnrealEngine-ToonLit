// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/CreateControlAssetRigSettings.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CreateControlAssetRigSettings)


UCreateControlPoseAssetRigSettings::UCreateControlPoseAssetRigSettings(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	AssetName = FString("ControlRigPose");
}

