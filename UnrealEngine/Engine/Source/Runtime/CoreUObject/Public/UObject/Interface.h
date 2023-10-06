// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

/**
 * Base class for all interfaces
 *
 */

class UInterface : public UObject
{
	DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(UInterface, UObject, CLASS_Interface | CLASS_Abstract, TEXT("/Script/CoreUObject"), CASTCLASS_None, COREUOBJECT_API)
};

class IInterface
{
protected:

	virtual ~IInterface() {}

public:

	typedef UInterface UClassType;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
