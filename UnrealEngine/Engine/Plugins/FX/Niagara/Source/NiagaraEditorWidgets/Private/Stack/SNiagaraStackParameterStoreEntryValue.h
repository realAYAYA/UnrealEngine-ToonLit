// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "AssetRegistry/AssetData.h"

class UNiagaraStackParameterStoreEntry;
class SNiagaraParameterEditor;
class FStructOnScope;
class SBox;
class IStructureDetailsView;

class SNiagaraStackParameterStoreEntryValue : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnColumnWidthChanged, float)
public:
	SLATE_BEGIN_ARGS(SNiagaraStackParameterStoreEntryValue) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraStackParameterStoreEntry* InStackEntry);

private:
	FReply DeleteClicked();

	TSharedRef<SWidget> ConstructValueStructWidget();

	void OnInputValueChanged();

	void ParameterBeginValueChange();

	void ParameterEndValueChange();

	void ParameterValueChanged(TSharedRef<SNiagaraParameterEditor> ParameterEditor);

	void ParameterPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent);

	EVisibility GetReferenceVisibility() const;

	EVisibility GetResetButtonVisibility() const;

	FReply ResetButtonPressed() const;

	FText GetInputIconText() const;

	FText GetInputIconToolTip() const;

	FSlateColor GetInputIconColor() const;

	void OnAssetSelectedFromPicker(const FAssetData& InAssetData, UClass* InClass);
	FString GetCurrentAssetPath() const;

private:
	UNiagaraStackParameterStoreEntry* StackEntry;

	TSharedPtr<FStructOnScope> DisplayedValueStruct;

	TSharedPtr<SBox> ValueStructContainer;
	TSharedPtr<SNiagaraParameterEditor> ValueStructParameterEditor;
	TSharedPtr<IStructureDetailsView> ValueStructDetailsView;
	const float TextIconSize = 16;
};