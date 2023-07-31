// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealClient.h"
#include "Types/SlateStructs.h"
#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class UStateTree;
class UStateTreeState;
class UStateTreeEditorData;
struct FStateTreeEditorPropertyPath;
enum class EStateTreeConditionOperand : uint8;

/**
 * Type customization for nodes (Conditions, Evaluators and Tasks) in StateTreeState.
 */
class FStateTreeEditorNodeDetails : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual ~FStateTreeEditorNodeDetails();
	
	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:

	bool ShouldResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle) const;
	void ResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle);
	void OnCopyNode();
	void OnPasteNode();
	TSharedPtr<IPropertyHandle> GetInstancedObjectValueHandle(TSharedPtr<IPropertyHandle> PropertyHandle);

	FOptionalSize GetIndentSize() const;
	TSharedRef<SWidget> OnGetIndentContent() const;

	int32 GetIndent() const;
	void SetIndent(const int32 Indent) const;
	bool IsIndent(const int32 Indent) const;
	
	FText GetOperandText() const;
	FSlateColor GetOperandColor() const;
	TSharedRef<SWidget> OnGetOperandContent() const;
	bool IsOperandEnabled() const;

	void SetOperand(const EStateTreeConditionOperand Operand) const;
	bool IsOperand(const EStateTreeConditionOperand Operand) const;

	bool IsFirstItem() const;
	int32 GetCurrIndent() const;
	int32 GetNextIndent() const;
	
	FText GetOpenParens() const;
	FText GetCloseParens() const;

	EVisibility IsConditionVisible() const;

	FText GetName() const;
	EVisibility IsNameVisible() const;
	void OnNameCommitted(const FText& NewText, ETextCommit::Type InTextCommit) const;
	bool IsNameEnabled() const;

	const struct FStateTreeEditorNode* GetCommonNode() const;
	FText GetDisplayValueString() const;
	const FSlateBrush* GetDisplayValueIcon() const;

	TSharedRef<SWidget> GeneratePicker();

	void OnStructPicked(const UScriptStruct* InStruct) const;
	void OnClassPicked(UClass* InClass) const;

	void OnIdentifierChanged(const UStateTree& StateTree);
	void OnBindingChanged(const FStateTreeEditorPropertyPath& SourcePath, const FStateTreeEditorPropertyPath& TargetPath);
	void FindOuterObjects();

	UScriptStruct* BaseScriptStruct = nullptr;
	UClass* BaseClass = nullptr;
	TSharedPtr<class SComboButton> ComboButton;

	UStateTreeEditorData* EditorData = nullptr;
	UStateTree* StateTree = nullptr;

	class IPropertyUtilities* PropUtils = nullptr;
	TSharedPtr<IPropertyHandle> StructProperty;
	TSharedPtr<IPropertyHandle> NodeProperty;
	TSharedPtr<IPropertyHandle> InstanceProperty;
	TSharedPtr<IPropertyHandle> InstanceObjectProperty;
	TSharedPtr<IPropertyHandle> IDProperty;

	TSharedPtr<IPropertyHandle> IndentProperty;
	TSharedPtr<IPropertyHandle> OperandProperty;

	FDelegateHandle OnBindingChangedHandle;
};
