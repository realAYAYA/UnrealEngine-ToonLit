// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Visual.generated.h"

/** The base class for elements in UMG: slots and widgets. */
UCLASS(DefaultToInstanced)
class UMG_API UVisual : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	virtual void ReleaseSlateResources(bool bReleaseChildren);

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	virtual bool NeedsLoadForServer() const override;
	//~ End UObject Interface

private:
	// Hide this to avoid confusion with UI selection state
	using UObject::IsSelected;
};
