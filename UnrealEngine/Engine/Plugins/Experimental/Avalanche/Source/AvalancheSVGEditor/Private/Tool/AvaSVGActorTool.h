// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/AvaInteractiveToolsActorPointToolBase.h"
#include "AvaSVGActorTool.generated.h"

UCLASS()
class UAvaSVGActorTool : public UAvaInteractiveToolsActorPointToolBase
{
	GENERATED_BODY()

public:
	UAvaSVGActorTool();

	//~ Begin UAvaInteractiveToolsToolBase
	virtual bool UseIdentityLocation() const override { return false; }
	virtual bool UseIdentityRotation() const override;
	virtual FName GetCategoryName() override;
	virtual FAvaInteractiveToolsToolParameters GetToolParameters() const override;
	//~ End UAvaInteractiveToolsToolBase
};
