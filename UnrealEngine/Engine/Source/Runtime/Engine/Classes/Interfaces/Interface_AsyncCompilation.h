// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Interface for objects that provide async compilation
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "Interface_AsyncCompilation.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UInterface_AsyncCompilation : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IInterface_AsyncCompilation
{
	GENERATED_IINTERFACE_BODY()

#if WITH_EDITOR
	/**	Returns whether or not the asset is currently being compiled */
	virtual bool IsCompiling() const = 0;
#endif
};

