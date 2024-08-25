// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/AvaInteractiveToolsActorTool.h"
#include "AvaCameraActorTool.generated.h"

UCLASS(DisplayName = "Motion Design Camera Actor Tool")
class UAvaCameraActorTool : public UAvaInteractiveToolsActorTool
{
	GENERATED_BODY()

public:
	UAvaCameraActorTool();

protected:
	//~ Begin UAvaInteractiveToolsToolBase
	virtual void DefaultAction() override;
	// End UAvaInteractiveToolsToolBase
};
