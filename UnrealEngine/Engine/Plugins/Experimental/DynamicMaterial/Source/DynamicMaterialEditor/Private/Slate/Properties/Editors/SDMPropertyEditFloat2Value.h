// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/SDMPropertyEdit.h"

class UDMMaterialValue;
class UDMMaterialValueFloat2;

class SDMPropertyEditFloat2Value : public SDMPropertyEdit
{
	SLATE_BEGIN_ARGS(SDMPropertyEditFloat2Value)
		{}
		SLATE_ARGUMENT(TSharedPtr<SDMComponentEdit>, ComponentEditWidget)
	SLATE_END_ARGS()

public:
	static TSharedPtr<SWidget> CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InFloat2Value);

	SDMPropertyEditFloat2Value() = default;
	virtual ~SDMPropertyEditFloat2Value() override = default;

	void Construct(const FArguments& InArgs, UDMMaterialValueFloat2* InFloat2Value);

	UDMMaterialValueFloat2* GetFloat2Value() const;

protected:
	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex) override;
	virtual float GetMaxWidthForWidget(int32 InIndex) const override;

	float GetSpinBoxValue(int32 InComponent) const;
	void OnSpinBoxValueChanged(float InNewValue, int32 InComponent) const;
};
