// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/SDMPropertyEdit.h"

class UDMMaterialValue;
class UDMMaterialValueFloat4;

class SDMPropertyEditFloat4Value : public SDMPropertyEdit
{
	SLATE_BEGIN_ARGS(SDMPropertyEditFloat4Value)
		{}
		SLATE_ARGUMENT(TSharedPtr<SDMComponentEdit>, ComponentEditWidget)
	SLATE_END_ARGS()

public:
	static TSharedPtr<SWidget> CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InFloat4Value);

	SDMPropertyEditFloat4Value() = default;
	virtual ~SDMPropertyEditFloat4Value() override = default;

	void Construct(const FArguments& InArgs, UDMMaterialValueFloat4* InFloat4Value);

	UDMMaterialValueFloat4* GetFloat4Value() const;

protected:
	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex) override;

	FLinearColor GetColor() const;
	void OnColorValueChanged(FLinearColor InNewColor) const;
};
