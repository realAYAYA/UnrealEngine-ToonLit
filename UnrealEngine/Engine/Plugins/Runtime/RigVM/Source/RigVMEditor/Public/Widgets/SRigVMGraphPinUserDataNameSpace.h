// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"
#include "Widgets/SRigVMGraphPinEditableNameValueWidget.h"

class RIGVMEDITOR_API SRigVMGraphPinUserDataNameSpace : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SRigVMGraphPinUserDataNameSpace) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	FText GetNameSpaceText() const;
	virtual void SetNameSpaceText(const FText& NewTypeInValue, ETextCommit::Type CommitInfo);

	TSharedRef<SWidget> MakeNameSpaceItemWidget(TSharedPtr<FString> InItem);
	void OnNameSpaceChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnNameSpaceComboBox();
	TArray<TSharedPtr<FString>>& GetNameSpaces();

	TArray<TSharedPtr<FString>> NameSpaces;
	TSharedPtr<SRigVMGraphPinEditableNameValueWidget> NameComboBox;

};
