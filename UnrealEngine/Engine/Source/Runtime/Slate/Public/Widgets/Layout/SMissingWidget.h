// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Like a null widget, but visualizes itself as being explicitly missing.
 */
class SMissingWidget
{
public:

	/**
	 * Creates a new instance.
	 *
	 * @return The widget.
	 */
	static SLATE_API TSharedRef<class SWidget> MakeMissingWidget( );
};
