// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"
#include "SSearchableComboBox.h"
#include "RigVMModel/RigVMPin.h"

class RIGVMEDITOR_API SRigVMEnumPicker : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal(TObjectPtr<UEnum>, FGetCurrentEnum);
	
	SLATE_BEGIN_ARGS(SRigVMEnumPicker){}
		SLATE_EVENT(SSearchableComboBox::FOnSelectionChanged, OnEnumChanged)
		SLATE_EVENT(FGetCurrentEnum, GetCurrentEnum)
		SLATE_ARGUMENT(bool, IsEnabled)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:

	void HandleControlEnumChanged(TSharedPtr<FString> InItem, ESelectInfo::Type InSelectionInfo)
	{
		if (OnEnumChangedDelegate.IsBound())
		{
			return OnEnumChangedDelegate.Execute(InItem, InSelectionInfo);
		}
	}

	UEnum* GetCurrentEnum()
	{
		if (GetCurrentEnumDelegate.IsBound())
		{
			return GetCurrentEnumDelegate.Execute();
		}
		return nullptr;
	}

	TSharedRef<SWidget> OnGetEnumNameWidget(TSharedPtr<FString> InItem);
	void PopulateEnumOptions();

	TArray<TSharedPtr<FString>> EnumOptions;
	SSearchableComboBox::FOnSelectionChanged OnEnumChangedDelegate;
	FGetCurrentEnum GetCurrentEnumDelegate;
	bool bIsEnabled;
};

class RIGVMEDITOR_API SRigVMGraphPinEnumPicker : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SRigVMGraphPinEnumPicker)
	: _ModelPin(nullptr) {}
		SLATE_ARGUMENT(URigVMPin*, ModelPin)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	void HandleControlEnumChanged(TSharedPtr<FString> InItem, ESelectInfo::Type InSelectionInfo);
	
	URigVMPin* ModelPin;

};
