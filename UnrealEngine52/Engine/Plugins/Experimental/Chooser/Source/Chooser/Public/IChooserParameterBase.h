// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
#include "IChooserParameterBase.generated.h"

USTRUCT()
struct FChooserParameterBase
{
	GENERATED_BODY()

	virtual void GetDisplayName(FText& OutName) const { }

	virtual ~FChooserParameterBase() {}
};