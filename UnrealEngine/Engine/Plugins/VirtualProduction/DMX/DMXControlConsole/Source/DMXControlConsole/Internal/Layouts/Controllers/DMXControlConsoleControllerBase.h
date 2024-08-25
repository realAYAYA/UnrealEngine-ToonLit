// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "DMXControlConsoleControllerBase.generated.h"


/** Base class for Controllers */
UCLASS(Abstract)
class DMXCONTROLCONSOLE_API UDMXControlConsoleControllerBase
	: public UObject
{
	GENERATED_BODY()

public:
	/** True if the value of the Controller can't be changed */
	bool IsLocked() const { return bIsLocked; }

	// Property Name getters
	FORCEINLINE static FName GetIsLockedPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleControllerBase, bIsLocked); }

protected:
	/** If true, the value of the Controller can't be changed */
	UPROPERTY(EditAnywhere, Category = "DMX Controller")
	bool bIsLocked = false;
};
