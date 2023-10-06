// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "IPropertyAccessEditor.h"

namespace ETextCommit { enum Type : int; }

class FMenuBuilder;
class UEdGraph;
class UBlueprint;
struct FEditorPropertyPath;

class SPropertyBinding : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SPropertyBinding){}

	SLATE_ARGUMENT(FPropertyBindingWidgetArgs, Args)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UBlueprint* InBlueprint, const TArray<FBindingContextStruct>& InBindingContextStructs);

protected:
	struct FFunctionInfo
	{
		FFunctionInfo()
			: Function(nullptr)
		{
		}

		FFunctionInfo(UFunction* InFunction)
			: DisplayName(InFunction->HasMetaData("ScriptName") ? InFunction->GetMetaDataText("ScriptName") : FText::FromName(InFunction->GetFName()))
			, Tooltip(InFunction->GetMetaData("Tooltip"))
			, FuncName(InFunction->GetFName())
			, Function(InFunction)
		{}

		FText DisplayName;
		FString Tooltip;

		FName FuncName;
		UFunction* Function;
	};

	TSharedRef<SWidget> OnGenerateDelegateMenu();
	void FillPropertyMenu(FMenuBuilder& MenuBuilder, UStruct* InOwnerStruct, TArray<TSharedPtr<FBindingChainElement>> InBindingChain);

	const FSlateBrush* GetCurrentBindingImage() const;
	FText GetCurrentBindingText() const;
	FText GetCurrentBindingToolTipText() const;
	FSlateColor GetCurrentBindingColor() const;

	bool CanRemoveBinding();
	void HandleRemoveBinding();

	void HandleAddBinding(TArray<TSharedPtr<FBindingChainElement>> InBindingChain);
	void HandleSetBindingArrayIndex(int32 InArrayIndex, ETextCommit::Type InCommitType, FProperty* InProperty, TArray<TSharedPtr<FBindingChainElement>> InBindingChain);

	void HandleCreateAndAddBinding();

	UStruct* ResolveIndirection(const TArray<TSharedPtr<FBindingChainElement>>& BindingChain) const;

	EVisibility GetGotoBindingVisibility() const;

	FReply HandleGotoBindingClicked();

	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

private:
	bool IsClassDenied(UClass* OwnerClass) const;
	bool IsFieldFromDeniedClass(FFieldVariant Field) const;
	bool HasBindableProperties(UStruct* InStruct, TArray<TSharedPtr<FBindingChainElement>>& BindingChain) const;
	bool HasBindablePropertiesRecursive(UStruct* InStruct, TSet<UStruct*>& VisitedStructs, TArray<TSharedPtr<FBindingChainElement>>& BindingChain) const;
	
	template <typename Predicate>
	void ForEachBindableProperty(UStruct* InStruct, Predicate Pred) const;

	template <typename Predicate>
	void ForEachBindableFunction(UClass* FromClass, Predicate Pred) const;

	UBlueprint* Blueprint;
	TArray<FBindingContextStruct> BindingContextStructs;
	FPropertyBindingWidgetArgs Args;
	FName PropertyName;
};
