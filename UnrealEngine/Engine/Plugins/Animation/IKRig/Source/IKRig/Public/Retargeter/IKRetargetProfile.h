// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Retargeter/IKRetargetSettings.h"

#include "IKRetargetProfile.generated.h"

class UIKRetargeter;
class URetargetChainSettings;

USTRUCT(BlueprintType)
struct FRetargetProfile
{
	GENERATED_BODY()
	
public:

	// If true, the TARGET Retarget Pose specified in this profile will be applied to the Retargeter (when plugged into the Retargeter).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RetargetPoses)
	bool bApplyTargetRetargetPose = false;
	
	// Override the TARGET Retarget Pose to use when this profile is active.
	// The pose must be present in the Retarget Asset and is not applied unless bApplyTargetRetargetPose is true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RetargetPoses, meta=(EditCondition="bApplyTargetRetargetPose"))
	FName TargetRetargetPoseName;

	// If true, the Source Retarget Pose specified in this profile will be applied to the Retargeter (when plugged into the Retargeter).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RetargetPoses)
	bool bApplySourceRetargetPose = false;

	// Override the SOURCE Retarget Pose to use when this profile is active.
	// The pose must be present in the Retarget Asset and is not applied unless bApplySourceRetargetPose is true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RetargetPoses, meta=(EditCondition="bApplySourceRetargetPose"))
	FName SourceRetargetPoseName;

	// If true, the Chain Settings stored in this profile will be applied to the Retargeter (when plugged into the Retargeter).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ChainSettings)
	bool bApplyChainSettings = true;
	
	// A (potentially sparse) set of setting overrides for the target chains (only applied when bApplyChainSettings is true).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ChainSettings, meta=(EditCondition="bApplyChainSettings"))
	TMap<FName, FTargetChainSettings> ChainSettings;

	// If true, the root settings stored in this profile will be applied to the Retargeter (when plugged into the Retargeter).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RootSettings)
	bool bApplyRootSettings = false;

	// Retarget settings to control behavior of the retarget root motion (not applied unless bApplyRootSettings is true)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RootSettings, meta=(EditCondition="bApplyRootSettings"))
	FTargetRootSettings RootSettings;

	// If true, the global settings stored in this profile will be applied to the Retargeter (when plugged into the Retargeter).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=GlobalSettings)
	bool bApplyGlobalSettings = false;

	// Retarget settings to control global behavior, like Stride Warping (not applied unless bApplyGlobalSettings is true)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=GlobalSettings, meta=(EditCondition="bApplyGlobalSettings"))
	FRetargetGlobalSettings GlobalSettings;
};
