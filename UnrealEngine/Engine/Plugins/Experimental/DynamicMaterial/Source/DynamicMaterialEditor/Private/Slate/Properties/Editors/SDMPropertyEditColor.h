// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/SDMPropertyEdit.h"
#include "Templates/SharedPointer.h"

class IPropertyHandle;
class SWidget;
struct FLinearColor;

class SDMPropertyEditColor : public SDMPropertyEdit
{
	SLATE_BEGIN_ARGS(SDMPropertyEditColor)
		{}
		SLATE_ARGUMENT(TSharedPtr<SDMComponentEdit>, ComponentEditWidget)
	SLATE_END_ARGS()

public:
	SDMPropertyEditColor() = default;
	virtual ~SDMPropertyEditColor() override = default;

	void Construct(const FArguments& InArgs, const TSharedPtr<IPropertyHandle>& InPropertyHandle);

protected:
	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex) override;

	FLinearColor GetColorValue() const;
	void OnColorChanged(FLinearColor InNewValue);
};
