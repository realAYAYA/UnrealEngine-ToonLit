// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactory.h"
#include "SwitchActorFactory.generated.h"

// The only purpose of this class is to trigger a slightly different code path within
// FLevelEditorViewportClient::TryPlacingActorFromObject when dragging and dropping a SwitchActor into the
// viewport, so that the SwitchActor labels get sanitized by FActorLabelUtilities::SetActorLabelUnique
// and don't repeatedly increment
UCLASS(MinimalAPI, config=Editor, collapsecategories, hidecategories=Object)
class USwitchActorFactory : public UActorFactory
{
	GENERATED_UCLASS_BODY()
};
