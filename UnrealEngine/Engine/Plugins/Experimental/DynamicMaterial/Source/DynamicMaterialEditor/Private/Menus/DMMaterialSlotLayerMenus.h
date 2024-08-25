// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageExpression.h"
#include "Templates/SharedPointer.h"

class SDMSlot;
class UDMMaterialLayerObject;
class UDMMaterialSlot;
class UToolMenu;

class FDMMaterialSlotLayerMenus
{
public:
	static UToolMenu* GenerateSlotLayerMenu(const TSharedPtr<SDMSlot>& InSlotWidget, UDMMaterialLayerObject* InLayerObject);

	static void AddAddLayerSection(UToolMenu* InMenu);
};
