// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/AvaInteractiveToolsActorPointToolBase.h"
#include "AvaInteractiveToolsActorToolNull.generated.h"

UCLASS()
class UAvaInteractiveToolsActorToolNull : public UAvaInteractiveToolsActorPointToolBase
{
	GENERATED_BODY()

public:
	UAvaInteractiveToolsActorToolNull();

	//~ Begin UAvaInteractiveToolsToolBase
	virtual bool UseIdentityLocation() const override { return false; }
	virtual bool UseIdentityRotation() const override { return true; }
	virtual FName GetCategoryName() override;
	virtual FAvaInteractiveToolsToolParameters GetToolParameters() const override;
	//~ End UAvaInteractiveToolsToolBase
};
