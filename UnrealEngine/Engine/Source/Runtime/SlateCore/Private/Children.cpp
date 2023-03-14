// Copyright Epic Games, Inc. All Rights Reserved.

#include "Layout/Children.h"

#include "Widgets/SNullWidget.h"


PRAGMA_DISABLE_DEPRECATION_WARNINGS
FNoChildren::FNoChildren()
	: FChildren(&SNullWidget::NullWidget.Get())
{

}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

