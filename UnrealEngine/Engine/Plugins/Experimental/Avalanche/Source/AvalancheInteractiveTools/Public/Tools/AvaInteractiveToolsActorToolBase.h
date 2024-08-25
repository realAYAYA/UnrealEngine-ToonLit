// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/AvaInteractiveToolsToolBase.h"
#include "Templates/SubclassOf.h"
#include "AvaInteractiveToolsActorToolBase.generated.h"

UCLASS(Abstract)
class AVALANCHEINTERACTIVETOOLS_API UAvaInteractiveToolsActorToolBase : public UAvaInteractiveToolsToolBase
{
	GENERATED_BODY()

	friend class UAvaInteractiveToolsActorToolBuilder;

protected:
	UPROPERTY()
	TSubclassOf<AActor> ActorClass = nullptr;

	//~ Begin UAvaInteractiveToolsToolBase
	virtual bool OnBegin() override;
	virtual void DefaultAction() override;
	//~ End UAvaInteractiveToolsToolBase
};
