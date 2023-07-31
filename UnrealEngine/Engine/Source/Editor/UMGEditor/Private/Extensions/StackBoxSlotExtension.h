// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "WidgetReference.h"
#include "DesignerExtension.h"

class FStackBoxSlotExtension : public FDesignerExtension
{
public:
	FStackBoxSlotExtension();

	virtual ~FStackBoxSlotExtension() {}

	virtual bool CanExtendSelection(const TArray< FWidgetReference >& Selection) const override;
	virtual void ExtendSelection(const TArray< FWidgetReference >& Selection, TArray< TSharedRef<FDesignerSurfaceElement> >& SurfaceElements) override;

private:
	FReply HandleShiftDirection(int32 ShiftAmount);
	void ShiftDirection(UWidget* Widget, int32 ShiftAmount);
};
