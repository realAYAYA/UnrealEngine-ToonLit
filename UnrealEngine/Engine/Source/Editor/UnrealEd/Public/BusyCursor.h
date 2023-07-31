// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * While in scope, sets the cursor to the busy (hourglass) cursor for all windows.
 */
class UNREALED_API FScopedBusyCursor
{
public:
	FScopedBusyCursor();
	~FScopedBusyCursor();
};
