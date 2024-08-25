// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMEDefs.h"
#include "Widgets/SCompoundWidget.h"

class SDMSlot;
class SWidget;

class SDMMaterialProperty : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDMMaterialProperty) {}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<SDMSlot>& InMaterialSlotWidget, EDMMaterialPropertyType InProperty);

protected:
	TWeakPtr<SDMSlot> MaterialSlotWidgetWeak;
	EDMMaterialPropertyType Property = EDMMaterialPropertyType::None;
};
