// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "NodeDependingOnEnumInterface.generated.h"

class UObject;

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class BLUEPRINTGRAPH_API UNodeDependingOnEnumInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class BLUEPRINTGRAPH_API INodeDependingOnEnumInterface
{
	GENERATED_IINTERFACE_BODY()

	virtual class UEnum* GetEnum() const PURE_VIRTUAL(INodeDependingOnEnumInterface::GetEnum,return NULL;);

	virtual void ReloadEnum(class UEnum*) PURE_VIRTUAL(INodeDependingOnEnumInterface::ReloadEnum, return;);

	virtual bool ShouldBeReconstructedAfterEnumChanged() const {return false;}
};
