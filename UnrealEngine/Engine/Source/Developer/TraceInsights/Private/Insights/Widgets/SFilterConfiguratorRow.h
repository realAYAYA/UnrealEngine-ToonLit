// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/Input/SComboBox.h"
#include "Widgets/Views/STableRow.h"

// Insights
#include "Insights/ViewModels/FilterConfiguratorNode.h"

namespace Insights
{

/** Widget that represents a table row in the Filter's tree control. Generates widgets for each column on demand. */
class SFilterConfiguratorRow : public SMultiColumnTableRow<FFilterConfiguratorNodePtr>
{
public:
	SFilterConfiguratorRow() {}
	~SFilterConfiguratorRow() {}

	SLATE_BEGIN_ARGS(SFilterConfiguratorRow) {}
		SLATE_ARGUMENT(FFilterConfiguratorNodePtr, FilterConfiguratorNodePtr)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override;

private:
	const TArray<TSharedPtr<struct FFilter>>* GetAvailableFilters();

	TSharedRef<SWidget> AvailableFilters_OnGenerateWidget(TSharedPtr<struct FFilter> InFilter);

	void AvailableFilters_OnSelectionChanged(TSharedPtr<struct FFilter> InFilter, ESelectInfo::Type SelectInfo);

	FText AvailableFilters_GetSelectionText() const;

	const TArray<TSharedPtr<class IFilterOperator>>* GetAvailableFilterOperators();

	TSharedRef<SWidget> AvailableFilterOperators_OnGenerateWidget(TSharedPtr<class IFilterOperator> InFilter);

	void AvailableFilterOperators_OnSelectionChanged(TSharedPtr<class IFilterOperator> InFilter, ESelectInfo::Type SelectInfo);

	FText AvailableFilterOperators_GetSelectionText() const;

	const TArray<TSharedPtr<struct FFilterGroupOperator>>* GetFilterGroupOperators();

	TSharedRef<SWidget> FilterGroupOperators_OnGenerateWidget(TSharedPtr<struct FFilterGroupOperator> InFilter);

	void FilterGroupOperators_OnSelectionChanged(TSharedPtr<struct FFilterGroupOperator> InFilter, ESelectInfo::Type SelectInfo);

	FText FilterGroupOperators_GetSelectionText() const;

	FReply AddFilter_OnClicked();
	FReply DeleteFilter_OnClicked();

	FReply AddGroup_OnClicked();
	FReply DeleteGroup_OnClicked();

	FText GetTextBoxValue() const;
	void OnTextBoxValueCommitted(const FText& InNewText, ETextCommit::Type InTextCommit);

	FText GetTextBoxTooltipText() const;
	FText GetTextBoxHintText() const;
	bool TextBox_OnVerifyTextChanged(const FText& InText, FText& OutErrorMessage);

	void SuggestionTextBox_GetSuggestions(const FString& Text, TArray<FString>& Suggestions);
	void SuggestionTextBox_GetHistory(TArray<FString>& Suggestions);

	void SuggestionTextBox_OnValueChanged(const FText& InNewText);
	FText SuggestionTextBox_GetValue() const;
	void SuggestionTextBox_OnValueCommitted(const FText& InNewText, ETextCommit::Type InTextCommit);

private:
	FFilterConfiguratorNodePtr FilterConfiguratorNodePtr;

	TSharedPtr<SComboBox<TSharedPtr<struct FFilter>>> FilterTypeComboBox;

	TSharedPtr<SComboBox<TSharedPtr<class IFilterOperator>>> FilterOperatorComboBox;

	TSharedPtr<SComboBox<TSharedPtr<struct FFilterGroupOperator>>> FilterGroupOperatorComboBox;

	FString SuggestionTextBoxValue;
};

} // namnespace Insights