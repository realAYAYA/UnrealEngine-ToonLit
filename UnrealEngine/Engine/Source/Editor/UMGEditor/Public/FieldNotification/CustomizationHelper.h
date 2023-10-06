// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Styling/SlateTypes.h"
#include "UObject/WeakFieldPtr.h"
#include "WidgetBlueprint.h"

class IDetailLayoutBuilder;
class IBlueprintEditor;
class UWidgetBlueprint;
class UK2Node_FunctionEntry;

namespace UE::FieldNotification
{

class UMGEDITOR_API FCustomizationHelper
{
public:
	static const FName MetaData_FieldNotify;

	FCustomizationHelper(UWidgetBlueprint* InBlueprint)
		: Blueprint(InBlueprint)
	{}

	void CustomizeVariableDetails(IDetailLayoutBuilder& DetailLayout);
	void CustomizeFunctionDetails(IDetailLayoutBuilder& DetailLayout);

private:
	ECheckBoxState IsPropertyFieldNotifyChecked() const;
	void HandlePropertyFieldNotifyCheckStateChanged(ECheckBoxState CheckBoxState);
	ECheckBoxState IsFunctionFieldNotifyChecked() const;
	void HandleFunctionFieldNotifyCheckStateChanged(ECheckBoxState CheckBoxState);

	UK2Node_FunctionEntry* FindFunctionEntry(UObject* Obj) const;

private:
	/** The blueprint we are editing */
	TWeakObjectPtr<UWidgetBlueprint> Blueprint;

	/** The property we are editing */
	TArray<TWeakFieldPtr<FProperty>> PropertiesBeingCustomized;

	/** The function we are editing */
	TArray<TWeakObjectPtr<UK2Node_FunctionEntry>> FunctionsBeingCustomized;
};

} //namespace