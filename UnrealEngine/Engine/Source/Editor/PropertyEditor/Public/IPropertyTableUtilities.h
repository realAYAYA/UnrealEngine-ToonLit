// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IPropertyUtilities.h"

class IPropertyTableUtilities : public IPropertyUtilities
{
public:

	virtual void RemoveColumn( const TSharedRef< class IPropertyTableColumn >& Column ) = 0;

};
