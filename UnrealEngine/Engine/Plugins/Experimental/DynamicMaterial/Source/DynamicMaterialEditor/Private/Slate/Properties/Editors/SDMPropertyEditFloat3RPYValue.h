// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/SDMPropertyEdit.h"

class UDMMaterialValue;
class UDMMaterialValueFloat3RPY;

class SDMPropertyEditFloat3RPYValue : public SDMPropertyEdit
{
	SLATE_BEGIN_ARGS(SDMPropertyEditFloat3RPYValue)
		{}
		SLATE_ARGUMENT(TSharedPtr<SDMComponentEdit>, ComponentEditWidget)
	SLATE_END_ARGS()

public:
	static TSharedPtr<SWidget> CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InFloat3RPYValue);

	SDMPropertyEditFloat3RPYValue() = default;
	virtual ~SDMPropertyEditFloat3RPYValue() override = default;

	void Construct(const FArguments& InArgs, UDMMaterialValueFloat3RPY* InFloat3RPYValue);

	UDMMaterialValueFloat3RPY* GetFloat3RPYValue() const;

protected:
	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex) override;
	virtual float GetMaxWidthForWidget(int32 InIndex) const override;

	float GetSpinBoxValue(EAxis::Type InAxis) const;
	void OnSpinBoxValueChanged(float InNewValue, EAxis::Type InAxis) const;
};
