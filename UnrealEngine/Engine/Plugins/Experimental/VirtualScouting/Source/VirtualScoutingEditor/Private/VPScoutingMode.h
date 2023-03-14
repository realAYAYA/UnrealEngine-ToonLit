// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VREditorMode.h"
#include "VPScoutingMode.generated.h"


UCLASS(meta=(DisplayName="Virtual Scouting Mode"))
class UVPScoutingMode : public UVREditorMode
{
	GENERATED_BODY()

public:
	UVPScoutingMode(const FObjectInitializer& ObjectInitializer);

	//~ Begin UVREditorMode interface
	virtual bool NeedsSyntheticDpad() override;

	virtual bool ShouldDisplayExperimentalWarningOnEntry() const override { return false; }

	virtual void Enter() override;
	//~ End UVREditorMode interface

protected:
	bool ValidateSettings();
};
