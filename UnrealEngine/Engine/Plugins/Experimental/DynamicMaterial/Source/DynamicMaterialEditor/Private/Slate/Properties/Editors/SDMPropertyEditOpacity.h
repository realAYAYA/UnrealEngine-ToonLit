// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "SDMPropertyEditFloat.h"

class UDMMaterialValueFloat1;
class SDMSlot;

class SDMPropertyEditOpacity : public SDMPropertyEditFloat
{
	SLATE_BEGIN_ARGS(SDMPropertyEditFloat)
		{}
	SLATE_END_ARGS()

public:
	using Super = SDMPropertyEditFloat;

	void Construct(const FArguments& InArgs, const TSharedPtr<SWidget>& InWidget, UDMMaterialValueFloat1* InOpacityValue);

protected:
	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex) override;
	virtual float GetMaxWidthForWidget(int32 InIndex) const override;
};
