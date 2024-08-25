// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/SDMPropertyEdit.h"
#include "Templates/SharedPointer.h"

class IPropertyHandle;
class SWidget;

class SDMPropertyEditFloat : public SDMPropertyEdit
{
	SLATE_BEGIN_ARGS(SDMPropertyEditFloat)
		: _FloatInterval(nullptr)
		{}
		SLATE_ARGUMENT(TSharedPtr<SDMComponentEdit>, ComponentEditWidget)
		SLATE_ARGUMENT(const FFloatInterval*, FloatInterval)
	SLATE_END_ARGS()

public:
	SDMPropertyEditFloat() = default;
	virtual ~SDMPropertyEditFloat() override = default;

	void Construct(const FArguments& InArgs, const TSharedPtr<IPropertyHandle>& InPropertyHandle);

protected:
	const FFloatInterval* FloatInterval;

	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex) override;
	virtual float GetMaxWidthForWidget(int32 InIndex) const override;

	virtual float GetFloatValue() const;
	void OnValueChanged(float InNewValue) const;
};
