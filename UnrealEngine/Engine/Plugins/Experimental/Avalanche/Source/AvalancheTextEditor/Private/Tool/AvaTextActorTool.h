// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/AvaInteractiveToolsActorPointToolBase.h"
#include "AvaTextActorTool.generated.h"

UCLASS()
class UAvaTextActorTool : public UAvaInteractiveToolsActorPointToolBase
{
	GENERATED_BODY()

public:
	UAvaTextActorTool();

	//~ Begin UAvaInteractiveToolsToolBase
	virtual bool UseIdentityLocation() const override { return false; }
	virtual bool UseIdentityRotation() const override;
	virtual FName GetCategoryName() override;
	virtual FAvaInteractiveToolsToolParameters GetToolParameters() const override;
	//~ End UAvaInteractiveToolsToolBase
};
