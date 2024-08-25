// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/SDMPropertyEdit.h"

class UDMMaterialValue;
class UDMMaterialValueFloat1;

class SDMPropertyEditFloat1Value : public SDMPropertyEdit
{
	SLATE_BEGIN_ARGS(SDMPropertyEditFloat1Value)
		{}
		SLATE_ARGUMENT(TSharedPtr<SDMComponentEdit>, ComponentEditWidget)
	SLATE_END_ARGS()

public:
	static TSharedPtr<SWidget> CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InFloat1Value);

	SDMPropertyEditFloat1Value() = default;
	virtual ~SDMPropertyEditFloat1Value() override = default;

	void Construct(const FArguments& InArgs, UDMMaterialValueFloat1* InFloat1Value);

	UDMMaterialValueFloat1* GetFloat1Value() const;

protected:
	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex) override;
	virtual float GetMaxWidthForWidget(int32 InIndex) const override;

	float GetSpinBoxValue() const;
	void OnSpinBoxValueChanged(float InNewValue) const;
};
