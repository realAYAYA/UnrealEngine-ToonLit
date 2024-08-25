// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/SDMPropertyEdit.h"
#include "Templates/SharedPointer.h"

class FString;
class IPropertyHandle;
class SWidget;

class SDMPropertyEditEnum : public SDMPropertyEdit
{
	SLATE_BEGIN_ARGS(SDMPropertyEditEnum)
		{}
		SLATE_ARGUMENT(TSharedPtr<SDMComponentEdit>, ComponentEditWidget)
	SLATE_END_ARGS()

public:
	SDMPropertyEditEnum() = default;
	virtual ~SDMPropertyEditEnum() override = default;

	void Construct(const FArguments& InArgs, const TSharedPtr<IPropertyHandle>& InPropertyHandle);

protected:
	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex)override;

	int64 GetEnumValue() const;
	FString GetEnumString() const;
	void GetEnumStrings(TArray<TSharedPtr<FString>>& OutComboBoxStrings, TArray<TSharedPtr<SToolTip>>& OutToolTips, TArray<bool>& OutRestrictedItems) const;
	void OnValueChanged(const FString& InNewValueString);
};
