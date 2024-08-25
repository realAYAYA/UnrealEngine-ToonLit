// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VREditorMode.h"
#include "VPScoutingMode.generated.h"


UCLASS(meta=(DisplayName="Legacy Virtual Scouting"))
class UVPScoutingMode : public UVREditorMode
{
	GENERATED_BODY()

public:
	UVPScoutingMode();

	//~ Begin UVREditorMode interface
	virtual bool NeedsSyntheticDpad() override;

	virtual void Enter() override;
	//~ End UVREditorMode interface

protected:
	bool ValidateSettings();

	void InvalidSettingNotification(const FText& ErrorDetails);
};
