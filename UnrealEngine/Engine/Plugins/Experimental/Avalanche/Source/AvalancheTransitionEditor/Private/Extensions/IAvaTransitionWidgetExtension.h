// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"
#include "Templates/SharedPointerFwd.h"

class SWidget;

/** Extension for a View Model that can supports creating its own widget */
class IAvaTransitionWidgetExtension
{
public:
	UE_AVA_TYPE(IAvaTransitionWidgetExtension)

	virtual TSharedRef<SWidget> CreateWidget() = 0;
};
