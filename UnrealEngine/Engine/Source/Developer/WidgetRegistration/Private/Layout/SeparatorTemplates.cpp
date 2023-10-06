// Copyright Epic Games, Inc. All Rights Reserved.

#include "Layout/SeparatorTemplates.h"

FSeparatorBuilder FSeparatorTemplates::SmallVerticalBackgroundNoBorder()
{
	return FSeparatorBuilder{ EStyleColor::Background, Orient_Vertical };
}

FSeparatorBuilder FSeparatorTemplates::SmallHorizontalBackgroundNoBorder()
{
	return FSeparatorBuilder{};
}

FSeparatorBuilder FSeparatorTemplates::SmallHorizontalPanelNoBorder()
{
	return FSeparatorBuilder{ EStyleColor::Panel };
}
