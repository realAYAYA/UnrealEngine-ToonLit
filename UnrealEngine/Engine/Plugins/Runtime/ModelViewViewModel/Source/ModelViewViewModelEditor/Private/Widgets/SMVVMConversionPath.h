// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

struct FCreateWidgetForActionData;
struct FEdGraphSchemaAction;
struct FGraphActionListBuilderBase;
class FMenuBuilder;
struct FMVVMBlueprintViewBinding;
class SMenuAnchor;
class UWidgetBlueprint;

class SMVVMConversionPath : public SCompoundWidget
{
private:
	struct FFunctionEntry
	{
		FString CategoryName;
		TArray<FFunctionEntry> Categories;
		TArray<const UFunction*> Functions;
	};

public:
	DECLARE_DELEGATE_OneParam(FOnFunctionChanged, const UFunction*);

	SLATE_BEGIN_ARGS(SMVVMConversionPath)
		{
		}
		SLATE_ATTRIBUTE(TArray<FMVVMBlueprintViewBinding*>, Bindings) 
		SLATE_EVENT(FOnFunctionChanged, OnFunctionChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const UWidgetBlueprint* InWidgetBlueprint, bool bInSourceToDestination);

private:
	EVisibility IsFunctionVisible() const;
	FText GetFunctionToolTip() const;
	FSlateColor GetFunctionColor() const;
	void SetConversionFunction(const UFunction* Function);
	TSharedRef<SWidget> GetFunctionMenuContent();
	FReply OnButtonClicked() const;
	FString GetFunctionPath() const;
	void PopulateMenuForEntry(FMenuBuilder& MenuBuilder, const FFunctionEntry* FunctionEntry);

private:

	FFunctionEntry RootEntry;
	TAttribute<TArray<FMVVMBlueprintViewBinding*>> Bindings;
	FOnFunctionChanged OnFunctionChanged;
	TSharedPtr<SMenuAnchor> Anchor;
	const UWidgetBlueprint* WidgetBlueprint = nullptr;
	bool bSourceToDestination = false;
};