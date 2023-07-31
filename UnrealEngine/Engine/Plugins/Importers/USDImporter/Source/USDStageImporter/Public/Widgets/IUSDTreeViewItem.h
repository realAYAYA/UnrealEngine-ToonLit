// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"

class USDSTAGEIMPORTER_API IUsdTreeViewItem : public TSharedFromThis< IUsdTreeViewItem >
{
public:
	virtual ~IUsdTreeViewItem() = default;
};
