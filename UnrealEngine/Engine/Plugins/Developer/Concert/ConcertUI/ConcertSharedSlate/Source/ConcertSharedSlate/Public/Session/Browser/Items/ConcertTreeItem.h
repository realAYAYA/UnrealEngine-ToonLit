// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Base class for everything displayed in session view */
class CONCERTSHAREDSLATE_API FConcertTreeItem
{
public:

	virtual void GetChildren(TArray<TSharedPtr<FConcertTreeItem>>& OutChildren) const = 0;
	
	virtual ~FConcertTreeItem() = default;
};