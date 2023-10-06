// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IObjectChooser.h"
#include "IChooserParameterBase.generated.h"

USTRUCT()
struct FChooserParameterBase
{
	GENERATED_BODY()

	virtual void GetDisplayName(FText& OutName) const { }

	virtual void PostLoad() {};

	virtual ~FChooserParameterBase() {}
};