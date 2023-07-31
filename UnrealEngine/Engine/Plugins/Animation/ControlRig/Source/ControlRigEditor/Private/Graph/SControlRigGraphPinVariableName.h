// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"
#include "Graph/SControlRigGraphPinEditableNameValueWidget.h"

class SControlRigGraphPinVariableName : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SControlRigGraphPinVariableName) {}
	SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	FText GetVariableNameText() const;
	virtual void SetVariableNameText(const FText& NewTypeInValue, ETextCommit::Type CommitInfo);

	TSharedRef<SWidget> MakeVariableNameItemWidget(TSharedPtr<FString> InItem);
	void OnVariableNameChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnVariableNameComboBox();
	TArray<TSharedPtr<FString>>& GetVariableNames();

	TArray<TSharedPtr<FString>> VariableNames;
	TSharedPtr<SControlRigGraphPinEditableNameValueWidget> NameComboBox;

};
