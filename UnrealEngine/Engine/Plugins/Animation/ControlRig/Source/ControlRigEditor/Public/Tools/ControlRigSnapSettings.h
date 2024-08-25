// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
/**
* Per Project Settings Animation Snapping
*/
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "BakingAnimationKeySettings.h"
#include "ControlRigSnapSettings.generated.h"


UCLASS(config = EditorPerProjectUserSettings)
class CONTROLRIGEDITOR_API UControlRigSnapSettings : public UObject
{

public:
	GENERATED_BODY()

	UControlRigSnapSettings();

	/** When snapping keep offset, If false snap completely */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Snap Settings", meta = (ToolTip = "When snapping keep offset, if false snap completely"))
	bool bKeepOffset = false;

	/** When snapping, also snap position */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Snap Settings", meta = (ToolTip = "When snapping, also snap position"))
	bool bSnapPosition = true;

	/** When snapping, also snap rotation */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Snap Settings", meta = (ToolTip = "When snapping, also snap rotation"))
	bool bSnapRotation = false;

	/** When snapping, also snap scale */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Snap Settings", meta = (ToolTip = "When snapping, also snap scale"))
	bool bSnapScale = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bake")
	EBakingKeySettings BakingKeySettings = EBakingKeySettings::KeysOnly;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bake", meta = (ClampMin = "1", UIMin = "1", EditCondition = "BakingKeySettings == EBakingKeySettings::AllFrames"))
	int32 FrameIncrement = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bake", meta = (EditCondition = "BakingKeySettings == EBakingKeySettings::AllFrames"))
	bool bReduceKeys = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bake", meta = (EditCondition = "BakingKeySettings == EBakingKeySettings::AllFrames || bReduceKeys"))
	float Tolerance = 0.001f;

};


