// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "Widgets/Views/STreeView.h"

struct FObjectKey;
struct FOptionalSize;

class FDetailWidgetRow;
class IDetailChildrenBuilder;
class IPropertyHandle;
class IPropertyUtilities;
class SComboButton;
class SWidget;
class SSearchBox;
class UStateTree;
class UStateTreeState;
class UStateTreeEditorData;
struct EVisibility;
struct FStateTreeEditorNode;
struct FStateTreePropertyPath;
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
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

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
	EVisibility IsTaskVisible() const;

	FText GetName() const;
	EVisibility IsNameVisible() const;
	void OnNameCommitted(const FText& NewText, ETextCommit::Type InTextCommit) const;
	bool IsNameEnabled() const;

	FText GetDisplayValueString() const;
	const FSlateBrush* GetDisplayValueIcon() const;

	TSharedRef<SWidget> GeneratePicker();
	void OnStructPicked(const UScriptStruct* InStruct) const;
	void OnClassPicked(const UClass* InClass) const;

	void OnIdentifierChanged(const UStateTree& StateTree);
	void OnBindingChanged(const FStateTreePropertyPath& SourcePath, const FStateTreePropertyPath& TargetPath);
	void FindOuterObjects();

	// Stores a category path segment, or a node type.
	struct FStateTreeNodeTypeItem
	{
		bool IsNode() const { return Struct != nullptr; }
		bool IsCategory() const { return CategoryPath.Num() > 0; }
		FString GetCategoryName() { return CategoryPath.Num() > 0 ? CategoryPath.Last() : FString(); }

		TArray<FString> CategoryPath;
		const UStruct* Struct = nullptr;
		TArray<TSharedPtr<FStateTreeNodeTypeItem>> Children;
	};

	// Stores per session node expansion state for a node type.
	struct FCategoryExpansionState
	{
		TSet<FString> ExpandedCategories;
	};

	static void SortNodeTypesFunctionItemsRecursive(TArray<TSharedPtr<FStateTreeNodeTypeItem>>& Items);
	static TSharedPtr<FStateTreeNodeTypeItem> FindOrCreateItemForCategory(TArray<TSharedPtr<FStateTreeNodeTypeItem>>& Items, TArrayView<FString> CategoryPath);
	void AddNode(const UStruct* Struct);
	void CacheNodeTypes();
	
	TSharedRef<ITableRow> GenerateNodeTypeRow(TSharedPtr<FStateTreeNodeTypeItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void GetNodeTypeChildren(TSharedPtr<FStateTreeNodeTypeItem> Item, TArray<TSharedPtr<FStateTreeNodeTypeItem>>& OutItems) const;
	void OnNodeTypeSelected(TSharedPtr<FStateTreeNodeTypeItem> SelectedItem, ESelectInfo::Type);
	void OnNodeTypeExpansionChanged(TSharedPtr<FStateTreeNodeTypeItem> ExpandedItem, bool bInExpanded);
	void OnSearchBoxTextChanged(const FText& NewText);
	int32 FilterNodeTypesChildren(const TArray<FString>& FilterStrings, const bool bParentMatches, const TArray<TSharedPtr<FStateTreeNodeTypeItem>>& SourceArray, TArray<TSharedPtr<FStateTreeNodeTypeItem>>& OutDestArray);
	void ExpandAll(const TArray<TSharedPtr<FStateTreeNodeTypeItem>>& Items);
	TArray<TSharedPtr<FStateTreeNodeTypeItem>> GetPathToItemStruct(const UStruct* Struct) const;

	void SaveExpansionState();
	void RestoreExpansionState();
	
	TSharedPtr<FStateTreeNodeTypeItem> RootNode;
	TSharedPtr<FStateTreeNodeTypeItem> FilteredRootNode;
	
	UScriptStruct* BaseScriptStruct = nullptr;
	UClass* BaseClass = nullptr;
	TSharedPtr<SComboButton> ComboButton;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<STreeView<TSharedPtr<FStateTreeNodeTypeItem>>> NodeTypeTree;
	bool bIsRestoringExpansion = false;

	UStateTreeEditorData* EditorData = nullptr;
	UStateTree* StateTree = nullptr;

	// Save expansion state for each base node type. The expansion state does not persist between editor sessions. 
	static TMap<FObjectKey, FCategoryExpansionState> CategoryExpansionStates;
	
	TSharedPtr<IPropertyUtilities> PropUtils;
	TSharedPtr<IPropertyHandle> StructProperty;
	TSharedPtr<IPropertyHandle> NodeProperty;
	TSharedPtr<IPropertyHandle> InstanceProperty;
	TSharedPtr<IPropertyHandle> InstanceObjectProperty;
	TSharedPtr<IPropertyHandle> IDProperty;

	TSharedPtr<IPropertyHandle> IndentProperty;
	TSharedPtr<IPropertyHandle> OperandProperty;

	FDelegateHandle OnBindingChangedHandle;
};
