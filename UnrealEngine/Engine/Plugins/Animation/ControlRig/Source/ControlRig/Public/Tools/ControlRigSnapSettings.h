// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
/**
* Per Project Settings Animation Snapping
*/
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ControlRig.h"
#include "ControlRigSnapSettings.generated.h"


UCLASS(config = EditorPerProjectUserSettings)
class CONTROLRIG_API UControlRigSnapSettings : public UObject
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

};


