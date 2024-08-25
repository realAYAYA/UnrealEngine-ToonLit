// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMBlueprintPin.h"
#include "MVVMPropertyPath.h"
#include "Types/MVVMLinkedPinValue.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SMVVMFieldSelectorMenu.h"

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
	SLATE_BEGIN_ARGS(SFunctionParameter) {}
		SLATE_ARGUMENT(FGuid, BindingId)
		SLATE_ARGUMENT(FMVVMBlueprintPinId, ParameterId)
		SLATE_ARGUMENT_DEFAULT(bool, SourceToDestination) = true;
		SLATE_ARGUMENT_DEFAULT(bool, AllowDefault) = true;
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UWidgetBlueprint* WidgetBlueprint);

private:
	ECheckBoxState OnGetIsBindArgumentChecked() const;
	void OnBindArgumentChecked(ECheckBoxState Checked);

	FMVVMLinkedPinValue OnGetSelectedField() const;
	void SetSelectedField(const FMVVMLinkedPinValue& Path);

	void HandleFieldSelectionChanged(FMVVMLinkedPinValue Value);
	FFieldSelectionContext GetSelectedSelectionContext() const;

	int32 GetCurrentWidgetIndex() const;

private:
	TWeakObjectPtr<UWidgetBlueprint> WidgetBlueprint;
	FGuid BindingId;
	FMVVMBlueprintPinId ParameterId;
	/** This reference is just to keep the default value widget alive. */
	TSharedPtr<SGraphPin> GraphPin;

	FMVVMLinkedPinValue PreviousSelectedField;

	bool bSourceToDestination = true;
	bool bAllowDefault = true;
	bool bDefaultValueVisible = true;
};

}
