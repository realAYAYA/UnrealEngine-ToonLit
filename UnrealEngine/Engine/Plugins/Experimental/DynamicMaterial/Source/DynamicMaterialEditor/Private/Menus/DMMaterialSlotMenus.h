// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class SDMSlot;
class SWidget;
class UToolMenu;

class FDMMaterialSlotMenus
{
public:
	static TSharedRef<SWidget> MakeAddLayerButtonMenu(const TSharedPtr<SDMSlot>& InSlotWidget);
};
