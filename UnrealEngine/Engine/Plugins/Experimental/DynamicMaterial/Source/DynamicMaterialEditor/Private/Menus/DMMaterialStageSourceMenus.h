// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"
#include "UObject/StrongObjectPtr.h"

class SDMSlot;
class SDMStage;
class SWidget;
class UDMMaterialStageExpression;
enum class EDMExpressionMenu : uint8;

class FDMMaterialStageSourceMenus
{
public:
	/** Generate right click menu for changing sources */
	static TSharedRef<SWidget> MakeChangeSourceMenu(const TSharedPtr<SDMSlot>& InSlotWidget, const TSharedPtr<SDMStage>& InStageWidget);

	static void CreateSourceMenuTree(TFunction<void(EDMExpressionMenu Menu, TArray<UDMMaterialStageExpression*>& SubmenuExpressionList)> Callback, 
		const TArray<TStrongObjectPtr<UClass>>& AllExpressions);
};
