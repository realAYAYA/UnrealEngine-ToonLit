// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Blueprint.h"
#include "UserWidgetBlueprint.generated.h"

UCLASS(Abstract, MinimalAPI)
class UUserWidgetBlueprint : public UBlueprint
{
	GENERATED_BODY()

public:
	/** Does the editor support widget from an editor package. */
	virtual bool AllowEditorWidget() const { return false; }
};
