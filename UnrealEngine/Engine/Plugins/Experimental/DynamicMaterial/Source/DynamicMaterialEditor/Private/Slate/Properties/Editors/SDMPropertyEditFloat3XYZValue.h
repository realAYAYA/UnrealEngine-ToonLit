// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/SDMPropertyEdit.h"

class UDMMaterialValue;
class UDMMaterialValueFloat3XYZ;

class SDMPropertyEditFloat3XYZValue : public SDMPropertyEdit
{
	SLATE_BEGIN_ARGS(SDMPropertyEditFloat3XYZValue)
		{}
		SLATE_ARGUMENT(TSharedPtr<SDMComponentEdit>, ComponentEditWidget)
	SLATE_END_ARGS()

public:
	static TSharedPtr<SWidget> CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InFloat3XYZValue);

	SDMPropertyEditFloat3XYZValue() = default;
	virtual ~SDMPropertyEditFloat3XYZValue() override = default;

	void Construct(const FArguments& InArgs, UDMMaterialValueFloat3XYZ* InFloat3XYZValue);

	UDMMaterialValueFloat3XYZ* GetFloat3XYZValue() const;

protected:
	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex) override;
	virtual float GetMaxWidthForWidget(int32 InIndex) const override;

	float GetSpinBoxValue(int32 InComponent) const;
	void OnSpinBoxValueChanged(float InNewValue, int32 InComponent) const;
};
