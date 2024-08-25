// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/SDMPropertyEdit.h"
#include "Templates/SharedPointer.h"

class FAssetThumbnailPool;
class UDMMaterialValue;
class UDMMaterialValueBool;
enum class ECheckBoxState : uint8;

class SDMPropertyEditBoolValue : public SDMPropertyEdit
{
	SLATE_BEGIN_ARGS(SDMPropertyEditBoolValue)
		{}
		SLATE_ARGUMENT(TSharedPtr<SDMComponentEdit>, ComponentEditWidget)
	SLATE_END_ARGS()

public:
	static TSharedPtr<SWidget> CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InBoolValue);

	SDMPropertyEditBoolValue() = default;
	virtual ~SDMPropertyEditBoolValue() override = default;

	void Construct(const FArguments& InArgs, UDMMaterialValueBool* InBoolValue);

	UDMMaterialValueBool* GetBoolValue() const;

protected:
	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex)override;

	ECheckBoxState IsChecked() const;
	void OnValueToggled(ECheckBoxState InNewState);
};
