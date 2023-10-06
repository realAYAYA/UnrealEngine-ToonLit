// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VPEditorTickableActorBase.h"

#include "VPTransientEditorTickableActorBase.generated.h"

/**
 * Specific VPEditorTickableActor explicitely marked as Transient
 */
UCLASS(Abstract, Transient)
class AVPTransientEditorTickableActorBase : public AVPEditorTickableActorBase
{
	GENERATED_BODY()
};
