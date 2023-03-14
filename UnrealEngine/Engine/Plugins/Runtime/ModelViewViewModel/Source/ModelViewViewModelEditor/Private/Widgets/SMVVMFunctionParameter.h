// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/DelegateCombinations.h"
#include "MVVMPropertyPath.h"
#include "Widgets/SCompoundWidget.h"

class SGraphPin;
class UK2Node_CallFunction;
class UWidgetBlueprint;

struct FMVVMBlueprintViewBinding;

enum class ECheckBoxState : uint8;
enum class EMVVMBindingMode : uint8;

namespace UE::MVVM
{

class SFieldSelector;

class SFunctionParameter : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal(EMVVMBindingMode, FGetBindingMode)

	SLATE_BEGIN_ARGS(SFunctionParameter) :
		_WidgetBlueprint(nullptr),
		_Binding(nullptr)
		{}
		SLATE_EVENT(FGetBindingMode, OnGetBindingMode)
		SLATE_ARGUMENT(UWidgetBlueprint*, WidgetBlueprint)
		SLATE_ARGUMENT(FMVVMBlueprintViewBinding*, Binding)
		SLATE_ARGUMENT(FName, ParameterName)
		SLATE_ARGUMENT_DEFAULT(bool, SourceToDestination) = true;
		SLATE_ARGUMENT_DEFAULT(bool, AllowDefault) = true;
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	ECheckBoxState OnGetIsBindArgumentChecked() const;
	void OnBindArgumentChecked(ECheckBoxState Checked);
	EVisibility OnGetVisibility(bool bDefaultValue) const;
	FMVVMBlueprintPropertyPath OnGetSelectedField() const;
	void OnFieldSelectionChanged(FMVVMBlueprintPropertyPath Selected);

private:

	TWeakObjectPtr<UWidgetBlueprint> WidgetBlueprint;
	FMVVMBlueprintViewBinding* Binding = nullptr;
	FName ParameterName;
	/** This reference is just to keep the default value widget alive. */
	TSharedPtr<SGraphPin> GraphPin;
	TSharedPtr<UE::MVVM::SFieldSelector> FieldSelector;
	FGetBindingMode GetBindingModeDelegate;

	bool bSourceToDestination = true;
	bool bAllowDefault = true;
};

}
