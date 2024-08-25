// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Types/SlateEnums.h"
#include "Widgets/Input/SComboBox.h"

#include "Insights/Common/SimpleRtti.h"
#include "Insights/ViewModels/Filters.h"

namespace TraceServices
{
	class ITimingProfilerTimerReader;
}

namespace Insights
{

class FTimerNameFilterState : public FFilterState
{
	INSIGHTS_DECLARE_RTTI(FTimerNameFilterState, FFilterState)

public:
	FTimerNameFilterState(TSharedRef<FFilter> InFilter)
		: FFilterState(InFilter)
	{}

	FTimerNameFilterState(const FTimerNameFilterState& Other)
		: FFilterState(Other)
	{
		FilterValue = Other.FilterValue;
	}

	virtual ~FTimerNameFilterState() {}

	virtual void Update() override;

	virtual bool ApplyFilter(const FFilterContext& Context) const override;

	virtual void SetFilterValue(FString InFilterValue) override { FilterValue = InFilterValue; }

	virtual bool Equals(const FFilterState& Other) const;
	virtual TSharedRef<FFilterState> DeepCopy() const;

private:
	FString FilterValue;
	TSet<uint32> TimerIds;
};

class FTimerNameFilter : public FCustomFilter
{
	INSIGHTS_DECLARE_RTTI(FTimerNameFilter, FCustomFilter)

public:
	FTimerNameFilter();

	virtual ~FTimerNameFilter() {}

	virtual TSharedRef<FFilterState> BuildFilterState() 
	{ 
		return MakeShared<FTimerNameFilterState>(SharedThis(this)); 
	}

	virtual TSharedRef<FFilterState> BuildFilterState(const FFilterState& Other)
	{
		return MakeShared<FTimerNameFilterState>(static_cast<const FTimerNameFilterState&>(Other));
	}

	void PopulateTimerNameSuggestionList(const FString& Text, TArray<FString>& OutSuggestions);
};

enum class EMetadataFilterDataType
{
	Bool = 1,
	Int = 2,
	Double = 3,
	String = 4,
};

struct FMetadataFilterDataTypeEntry
{
	FMetadataFilterDataTypeEntry(EMetadataFilterDataType InType, FText InName)
	{
		Type = InType;
		Name = InName;
	}

	EMetadataFilterDataType Type;
	FText Name;
};

class FMetadataFilterState : public FFilterState, public TSharedFromThis<FMetadataFilterState>
{
	INSIGHTS_DECLARE_RTTI(FMetadataFilterState, FFilterState)

public:
	FMetadataFilterState(TSharedRef<FFilter> InFilter);

	virtual ~FMetadataFilterState() {}

	virtual void Update() override;

	virtual bool ApplyFilter(const FFilterContext& Context) const override;

	virtual void SetFilterValue(FString InFilterValue) override {}

	virtual bool HasCustomUI() const override { return true; }
	virtual void AddCustomUI(TSharedRef<SHorizontalBox> LeftBox) override;

	virtual bool Equals(const FFilterState& Other) const override ;
	virtual TSharedRef<FFilterState> DeepCopy() const override ;

private:
	FText GetKeyTextBoxValue() const;
	void OnKeyTextBoxValueCommitted(const FText& InNewText, ETextCommit::Type InTextCommit);

	TSharedRef<SWidget> DataType_OnGenerateWidget(TSharedPtr<FMetadataFilterDataTypeEntry> InDataType);
	void DataType_OnSelectionChanged(TSharedPtr<FMetadataFilterDataTypeEntry> InDataType, ESelectInfo::Type SelectInfo);
	FText DataType_GetSelectionText() const;

	TSharedRef<SWidget> AvailableOperators_OnGenerateWidget(TSharedPtr<IFilterOperator> InOperator);
	void AvailableOperators_OnSelectionChanged(TSharedPtr<IFilterOperator> InOperator, ESelectInfo::Type SelectInfo);
	FText AvailableOperators_GetSelectionText() const;

	FText GetTermTextBoxValue() const;
	void OnTermTextBoxValueCommitted(const FText& InNewText, ETextCommit::Type InTextCommit);

	bool ApplyFilterToMetadata(TArrayView<const uint8>& Metadata) const;

private:
	FString Key;
	FString Term;

	typedef TVariant<double, int64, bool> ConvertedDataVariant;
	ConvertedDataVariant ConvertedData;

	bool bShowAllMetadataEvents = false;
	
	TArray<TSharedPtr<FMetadataFilterDataTypeEntry>> AvailableDataTypes;
	TSharedPtr<FMetadataFilterDataTypeEntry> SelectedDataType;
	
	TArray<TSharedPtr<IFilterOperator>> AvailableOperators;
	TArray<TSharedPtr<IFilterOperator>> BoolOperators;

	TSharedPtr<SComboBox<TSharedPtr<IFilterOperator>>> OperatorComboBox;

	const TraceServices::ITimingProfilerTimerReader* TimerReader;
};

class FMetadataFilter : public FFilter
{
	INSIGHTS_DECLARE_RTTI(FMetadataFilter, FFilter)

public:
	FMetadataFilter();

	virtual ~FMetadataFilter() {}

	virtual TSharedRef<FFilterState> BuildFilterState()
	{
		return MakeShared<FMetadataFilterState>(SharedThis(this));
	}

	virtual TSharedRef<FFilterState> BuildFilterState(const FFilterState& Other)
	{
		return MakeShared<FMetadataFilterState>(static_cast<const FMetadataFilterState&>(Other));
	}
};

} // namespace Insights
