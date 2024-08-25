// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMDefs.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SExpandableArea;
class SWidget;
class UDynamicMaterialModel;

class SDMMaterialParameters : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDMMaterialParameters) {}
	SLATE_END_ARGS()

public:
	virtual ~SDMMaterialParameters() {}

	void Construct(const FArguments& InArgs, TWeakObjectPtr<UDynamicMaterialModel> InModelWeak);

	void RefreshWidgets();

protected:
	TWeakObjectPtr<UDynamicMaterialModel> ModelWeak;

	TSharedPtr<SExpandableArea> ValuesArea;
	TSharedPtr<SBox> AreaBody;

	TSharedRef<SWidget> CreateValuesWidget();

	FReply OnAddValueButtonClicked(EDMValueType Type);
	FReply OnRemoveValueButtonClicked(int32 Index);
	bool OnVerifyValueNameChanged(const FText& InNewName, FText& OutErrorText, int32 ValueIndex);
	void OnAcceptValueNameChanged(const FText& InNewName, int32 ValueIndex);
};
