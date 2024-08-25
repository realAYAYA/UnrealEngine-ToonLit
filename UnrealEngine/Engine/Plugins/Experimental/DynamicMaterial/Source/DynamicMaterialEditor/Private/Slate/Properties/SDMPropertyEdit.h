// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DynamicMaterialEditorModule.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SCompoundWidget.h"

class FScopedTransaction;
class SDMComponentEdit;
class SHorizontalBox;
class UDMMaterialValue;
enum class ECheckBoxState : uint8;

/**
 * Material Designer Property Edit
 * 
 * Editor widget for modifying the value of a material property.
 * Can accept a property in the form of a UDMMaterialValue or TSharedPtr<IPropertyHandle>.
*/
class SDMPropertyEdit : public SCompoundWidget
{
public:
	static FReply CreateRightClickDetailsMenu(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent,
		TWeakPtr<SDMPropertyEdit> InPropertyEditorWeak);

	SLATE_BEGIN_ARGS(SDMPropertyEdit)
		: _InputCount(1)
		{}
		SLATE_ARGUMENT(int32, InputCount)
		SLATE_ARGUMENT(TSharedPtr<SDMComponentEdit>, ComponentEditWidget)
		SLATE_ARGUMENT(TSharedPtr<IPropertyHandle>, PropertyHandle)
		SLATE_ARGUMENT(TWeakObjectPtr<UDMMaterialValue>, PropertyMaterialValue)
	SLATE_END_ARGS()

	static TSharedRef<SHorizontalBox> AddWidgetLabel(const FText& InLabel, TSharedRef<SWidget> ValueWidget, TSharedPtr<SHorizontalBox> AddOnToWidget = nullptr);

	virtual ~SDMPropertyEdit() {}

	void Construct(const FArguments& InArgs);

	UDMMaterialValue* GetValue() const;

	TSharedPtr<IPropertyHandle> GetPropertyHandle() const;

protected:
	TWeakPtr<SDMComponentEdit> ComponentEditWidgetWeak;
	TSharedPtr<IPropertyHandle> PropertyHandle;
	TWeakObjectPtr<UDMMaterialValue> PropertyMaterialValue;

	FProperty* Property = nullptr;
	TSharedPtr<FScopedTransaction> ScrubbingTransaction;

	static void CreateKey(TWeakObjectPtr<UDMMaterialValue> InValueWeak);

	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex);
	virtual float GetMaxWidthForWidget(int32 InIndex) const;
 
	TSharedRef<SWidget> CreateCheckbox(const TAttribute<ECheckBoxState>& InValueAttr, const FOnCheckStateChanged& InChangeFunc);
 
	TSharedRef<SWidget> CreateAssetPicker(UClass* InAllowedClass, const TAttribute<FString>& InPathAttr, const FOnSetObject& InChangeFunc);
 
	void OnSpinBoxStartScrubbing(FText InTransactionDescription);
	void OnSpinBoxEndScrubbing(float InValue);

	void OnSpinBoxValueChange(float InValue, const SSpinBox<float>::FOnValueChanged InChangeFunc);

	void OnSpinBoxCommit(float InValue, ETextCommit::Type InCommitType, const SSpinBox<float>::FOnValueChanged InChangeFunc,
		FText InTransactionDescription);
 
	TSharedRef<SSpinBox<float>> CreateSpinBox(const TAttribute<float>& InValue, const SSpinBox<float>::FOnValueChanged& InOnValueChanged,
		const FText& InTransactionDescription, const FFloatInterval* InValueRange = nullptr);
 
	void OnColorPickedCommitted(FLinearColor InNewColor, const FOnLinearColorValueChanged InChangeFunc);
	void OnColorPickerCancelled(FLinearColor InOldColor, const FOnLinearColorValueChanged InChangeFunc);
	void OnColorPickerClosed(const TSharedRef<SWindow>& InClosedWindow);
 
	/** @param CommitFunc must call EndTransaction() */
	TSharedRef<SWidget> CreateColorPicker(bool bInUseAlpha, const TAttribute<FLinearColor>& InValueAttr,
		const FOnLinearColorValueChanged& InChangeFunc, const FText& InPickerTransationDescription,
		const FFloatInterval* InValueRange = nullptr);

	TSharedRef<SWidget> CreateEnum(const FOnGetPropertyComboBoxValue& InValueString, const FOnGetPropertyComboBoxStrings& InGetStrings,
		const FOnPropertyComboBoxValueSelected& InValueSet);

	FReply OnClickColorBlock(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bInseAlpha,
		TAttribute<FLinearColor> InValueAttr, FOnLinearColorValueChanged InChangeFunc, FText InPickerTransactionDescription);
 
	virtual void StartTransaction(FText InDescription);
	virtual void EndTransaction();
 
	TSharedRef<SWidget> MakeCopyPasteMenu(const TSharedPtr<IPropertyHandle>& InPropertyHandle) const;
};
