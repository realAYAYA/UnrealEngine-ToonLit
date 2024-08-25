// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class SDMSlot;
class SDMStage;
class UToolMenu;

class FDMMaterialStageMenus
{
public:
	static FName GetStageMenuName();

	static FName GetStageToggleSectionName();

	static UToolMenu* GenerateStageMenu(const TSharedPtr<SDMSlot>& InSlotWidget, const TSharedPtr<SDMStage>& InStageWidget);

	static void AddStageSection(UToolMenu* InMenu);
};
