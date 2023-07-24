// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldConditionBase.h"
#include "SmartObjectWorldConditionBase.generated.h"

/**
 * Base struct for all conditions accepted by Smart Objects.
 */
USTRUCT(meta=(Hidden))
struct SMARTOBJECTSMODULE_API FSmartObjectWorldConditionBase : public FWorldConditionBase
{
	GENERATED_BODY()
};