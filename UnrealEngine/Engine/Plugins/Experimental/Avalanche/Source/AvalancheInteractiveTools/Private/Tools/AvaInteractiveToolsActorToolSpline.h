// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/AvaInteractiveToolsActorPointToolBase.h"
#include "AvaInteractiveToolsActorToolSpline.generated.h"

UCLASS()
class UAvaInteractiveToolsActorToolSpline : public UAvaInteractiveToolsActorPointToolBase
{
	GENERATED_BODY()

public:
	UAvaInteractiveToolsActorToolSpline();

	//~ Begin UAvaInteractiveToolsToolBase
	virtual bool UseIdentityLocation() const override { return false; }
	virtual bool UseIdentityRotation() const override { return false; }
	virtual FName GetCategoryName() override;
	virtual FAvaInteractiveToolsToolParameters GetToolParameters() const override;
	//~ End UAvaInteractiveToolsToolBase
};
