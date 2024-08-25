// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/SDMPropertyEdit.h"

class UDMMaterialValue;
class UDMMaterialValueFloat3RGB;

class SDMPropertyEditFloat3RGBValue : public SDMPropertyEdit
{
	SLATE_BEGIN_ARGS(SDMPropertyEditFloat3RGBValue)
		{}
		SLATE_ARGUMENT(TSharedPtr<SDMComponentEdit>, ComponentEditWidget)
	SLATE_END_ARGS()

public:
	static TSharedPtr<SWidget> CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InFloat3RGBValue);

	SDMPropertyEditFloat3RGBValue() = default;
	virtual ~SDMPropertyEditFloat3RGBValue() override = default;

	void Construct(const FArguments& InArgs, UDMMaterialValueFloat3RGB* InFloat3RGBValue);

	UDMMaterialValueFloat3RGB* GetFloat3RGBValue() const;

protected:
	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex) override;

	FLinearColor GetColor() const;
	void OnColorValueChanged(FLinearColor InNewColor) const;
};
