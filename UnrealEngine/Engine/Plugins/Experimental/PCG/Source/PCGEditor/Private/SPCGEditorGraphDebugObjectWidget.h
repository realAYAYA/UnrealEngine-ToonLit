// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/STextComboBox.h"

class FPCGEditor;
class UPCGComponent;

struct FPCGEditorGraphDebugObjectInstance
{
	FPCGEditorGraphDebugObjectInstance(const FString& InLabel)
		: PCGComponent(nullptr)
		, Label(InLabel)
	{
	}
	
	FPCGEditorGraphDebugObjectInstance(TWeakObjectPtr<UPCGComponent> InPCGComponent, const FString& InLabel)
		: PCGComponent(InPCGComponent)
		, Label(InLabel)
	{
	}

	FText GetDebugObjectText() const
	{
		return FText::FromString(Label);
	}

	TWeakObjectPtr<UPCGComponent> PCGComponent;
	FString Label;
};


class SPCGEditorGraphDebugObjectWidget: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphDebugObjectWidget) { }
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor);

private:
	void OnComboBoxOpening();
	void OnSelectionChanged(TSharedPtr<FPCGEditorGraphDebugObjectInstance> NewSelection, ESelectInfo::Type SelectInfo) const;
	TSharedRef<SWidget> OnGenerateWidget(TSharedPtr<FPCGEditorGraphDebugObjectInstance> InDebugObjectInstance) const;

	FText GetSelectedDebugObjectText() const;
	void SelectedDebugObject_OnClicked() const;
	bool IsSelectDebugObjectButtonEnabled() const;

	/** Pointer back to the PCG editor that owns us */
	TWeakPtr<FPCGEditor> PCGEditorPtr;

	TArray<TSharedPtr<FPCGEditorGraphDebugObjectInstance>> DebugObjects;
	TSharedPtr<SComboBox<TSharedPtr<FPCGEditorGraphDebugObjectInstance>>> DebugObjectsComboBox;
};
