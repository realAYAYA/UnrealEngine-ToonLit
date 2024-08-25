// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimerFilters.h"

#include "CborReader.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"

#include "Insights/TimingProfilerManager.h"
#include "Insights/ViewModels/ThreadTimingTrack.h"

#define LOCTEXT_NAMESPACE "Insights::TimerFilters"

namespace Insights
{

INSIGHTS_IMPLEMENT_RTTI(FTimerNameFilterState)
INSIGHTS_IMPLEMENT_RTTI(FTimerNameFilter)

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTimerNameFilterState
////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimerNameFilterState::Update()
{
	if (FilterValue.IsEmpty() || !SelectedOperator.IsValid())
	{
		return;
	}

	TimerIds.Empty();
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid() && TraceServices::ReadTimingProfilerProvider(*Session.Get()))
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

		const TraceServices::ITimingProfilerTimerReader* TimerReader;
		TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

		uint32 TimerCount = TimerReader->GetTimerCount();
		for (uint32 TimerIndex = 0; TimerIndex < TimerCount; ++TimerIndex)
		{
			const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(TimerIndex);
			if (Timer && Timer->Name)
			{
				if (SelectedOperator->GetKey() == EFilterOperator::Eq)
				{
					if (FCString::Stricmp(Timer->Name, *FilterValue) == 0)
					{
						TimerIds.Add(Timer->Id);
					}
				}
				else if (SelectedOperator->GetKey() == EFilterOperator::Contains)
				{
					if (FCString::Stristr(Timer->Name, *FilterValue))
					{
						TimerIds.Add(Timer->Id);
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FTimerNameFilterState::ApplyFilter(const FFilterContext& Context) const
{
	if (!Context.HasFilterData(static_cast<int32>(Filter->GetKey())))
	{
		return Context.GetReturnValueForUnsetFilters();
	}

	int64 Value;
	Context.GetFilterData<int64>(Filter->GetKey(), Value);

	return TimerIds.Contains(static_cast<uint32>(Value));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FTimerNameFilterState::Equals(const FFilterState& Other) const
{
	if (this->GetTypeName() != Other.GetTypeName())
	{
		return false;
	}

	const FTimerNameFilterState& OtherTimerNameFilter = StaticCast<const FTimerNameFilterState&>(Other);
	return FilterValue == OtherTimerNameFilter.FilterValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<FFilterState> FTimerNameFilterState::DeepCopy() const
{
	TSharedRef<FTimerNameFilterState> Copy = MakeShared<FTimerNameFilterState>(*this);

	return Copy;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTimerNameFilter
////////////////////////////////////////////////////////////////////////////////////////////////////

FTimerNameFilter::FTimerNameFilter()
	: FCustomFilter(static_cast<int32>(EFilterField::TimerName),
		LOCTEXT("TimerName", "Timer Name"),
		LOCTEXT("TimerName", "Timer Name"),
		EFilterDataType::Custom,
		nullptr,
		nullptr)
{
	SupportedOperators = MakeShared<TArray<TSharedPtr<IFilterOperator>>>();
	SupportedOperators->Add(StaticCastSharedRef<IFilterOperator>(MakeShared<FFilterOperator<int64>>(EFilterOperator::Eq, TEXT("Is"), [](int64 lhs, int64 rhs) { return lhs == rhs; })));
	SupportedOperators->Add(StaticCastSharedRef<IFilterOperator>(MakeShared<FFilterOperator<int64>>(EFilterOperator::Contains, TEXT("Contains"), [](int64 lhs, int64 rhs) { return lhs == rhs; })));

	SetCallback([this](const FString& Text, TArray<FString>& OutSuggestions)
		{
			this->PopulateTimerNameSuggestionList(Text, OutSuggestions);
		});
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimerNameFilter::PopulateTimerNameSuggestionList(const FString& Text, TArray<FString>& OutSuggestions)
{
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid() && TraceServices::ReadTimingProfilerProvider(*Session.Get()))
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

		const TraceServices::ITimingProfilerTimerReader* TimerReader;
		TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

		uint32 TimerCount = TimerReader->GetTimerCount();
		for (uint32 TimerIndex = 0; TimerIndex < TimerCount; ++TimerIndex)
		{
			const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(TimerIndex);
			if (Timer && Timer->Name)
			{
				if (Text.IsEmpty())
				{
					OutSuggestions.Add(Timer->Name);
					continue;
				}
				const TCHAR* FoundString = FCString::Stristr(Timer->Name, *Text);
				if (FoundString)
				{
					OutSuggestions.Add(Timer->Name);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FMetadataFilterState
////////////////////////////////////////////////////////////////////////////////////////////////////

INSIGHTS_IMPLEMENT_RTTI(FMetadataFilterState)

FMetadataFilterState::FMetadataFilterState(TSharedRef<FFilter> InFilter)
	: FFilterState(InFilter)
{
	AvailableDataTypes.Add(MakeShared<FMetadataFilterDataTypeEntry>(EMetadataFilterDataType::Int, LOCTEXT("IntDataType", "Int")));
	AvailableDataTypes.Add(MakeShared<FMetadataFilterDataTypeEntry>(EMetadataFilterDataType::Double, LOCTEXT("DoubleDataType", "Double")));
	AvailableDataTypes.Add(MakeShared<FMetadataFilterDataTypeEntry>(EMetadataFilterDataType::Bool, LOCTEXT("BoolDataType", "Bool")));
	AvailableDataTypes.Add(MakeShared<FMetadataFilterDataTypeEntry>(EMetadataFilterDataType::String, LOCTEXT("StringDataType", "String")));

	SelectedDataType = AvailableDataTypes[0];
	DataType_OnSelectionChanged(SelectedDataType, ESelectInfo::Type::Direct);

	BoolOperators.Add((MakeShared<FFilterOperator<bool>>(EFilterOperator::Eq, TEXT("IS"), [](bool lhs, bool rhs) { return lhs == rhs; })));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMetadataFilterState::AddCustomUI(TSharedRef<SHorizontalBox> Box)
{
	Box->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(FMargin(4.0f, 0.0f, 4.0f, 0.0f))
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(200.0f)
			.OnTextCommitted(this, &FMetadataFilterState::OnKeyTextBoxValueCommitted)
			.Text(this, &FMetadataFilterState::GetKeyTextBoxValue)
			.ToolTipText(LOCTEXT("MetadataKeyTooltipText", "The key value for the metadata."))
			.HintText(LOCTEXT("MetadataKeyHint", "The key value for the metadata."))
		];

	Box->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(FMargin(0.0f, 2.0f))
		[
			SNew(SComboBox<TSharedPtr<FMetadataFilterDataTypeEntry>>)
			.OptionsSource(&AvailableDataTypes)
			.OnSelectionChanged(this, &FMetadataFilterState::DataType_OnSelectionChanged)
			.OnGenerateWidget(this, &FMetadataFilterState::DataType_OnGenerateWidget)
			[
				SNew(STextBlock)
				.Text(this, &FMetadataFilterState::DataType_GetSelectionText)
			]
		];

		Box->AddSlot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.FillWidth(1.0)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.AutoWidth()
			[
				SAssignNew(OperatorComboBox, SComboBox<TSharedPtr<IFilterOperator>>)
				.OptionsSource(&AvailableOperators)
				.OnSelectionChanged(this, &FMetadataFilterState::AvailableOperators_OnSelectionChanged)
				.OnGenerateWidget(this, &FMetadataFilterState::AvailableOperators_OnGenerateWidget)
				[
					SNew(STextBlock)
					.Text(this, &FMetadataFilterState::AvailableOperators_GetSelectionText)
				]
			];

		Box->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.FillWidth(1.0)
		.Padding(FMargin(4.0f, 0.0f, 4.0f, 0.0f))
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(200.0f)
			.OnTextCommitted(this, &FMetadataFilterState::OnTermTextBoxValueCommitted)
			.Text(this, &FMetadataFilterState::GetTermTextBoxValue)
			.ToolTipText(LOCTEXT("MetadataValueTooltipText", "The value of the metadata field."))
			.HintText(LOCTEXT("MetadataValueHint", "The value for the metadata field."))
		];
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMetadataFilterState::Update()
{
	bShowAllMetadataEvents = false;
	if (Key.Equals(TEXT("*")))
	{
		bShowAllMetadataEvents = true;
	}

	ConvertedData = ConvertedDataVariant();
	
	ensure(SelectedDataType.IsValid());
	switch (SelectedDataType->Type)
	{
	case EMetadataFilterDataType::Bool:
	{
		ConvertedData.Set<bool>(FCString::ToBool(*Term));
		break;
	}
	case EMetadataFilterDataType::Int:
	{
		ConvertedData.Set<int64>(FCString::Atoi64(*Term));
		break;
	}
	case EMetadataFilterDataType::Double:
	{
		ConvertedData.Set<double>(FCString::Atod(*Term));
		break;
	}
	}

	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid() && TraceServices::ReadTimingProfilerProvider(*Session.Get()))
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

		// We can cache the timer reader because ApplyFilters needs to be called from inside a session lock. Acquiring the lock
		// in ApplyFilter is too expensive.
		TimingProfilerProvider.ReadTimers([this](const TraceServices::ITimingProfilerTimerReader& Out) { this->TimerReader = &Out; });
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FMetadataFilterState::ApplyFilter(const FFilterContext& Context) const
{
	if (!Context.HasFilterData(static_cast<int32>(Filter->GetKey())))
	{
		return Context.GetReturnValueForUnsetFilters();
	}

	int64 TimerIndex;
	Context.GetFilterData<int64>(Filter->GetKey(), TimerIndex);

	TArrayView<const uint8> Metadata = TimerReader->GetMetadata(static_cast<uint32>(TimerIndex));
	if (Metadata.Num() > 0)
	{
		if (!bShowAllMetadataEvents)
		{
			return ApplyFilterToMetadata(Metadata);
		}

		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FMetadataFilterState::ApplyFilterToMetadata(TArrayView<const uint8>& Metadata) const
{
	FMemoryReaderView MemoryReader(Metadata);
	FCborReader CborReader(&MemoryReader, ECborEndianness::StandardCompliant);
	FCborContext Context;

	if (!CborReader.ReadNext(Context) || Context.MajorType() != ECborCode::Map)
	{
		return false;
	}

	ensure(SelectedDataType);

	while (true)
	{
		// Read key
		if (!CborReader.ReadNext(Context) || !Context.IsString())
		{
			break;
		}

		FString CurrentKey(static_cast<int32>(Context.AsLength()), Context.AsCString());

		if (!CborReader.ReadNext(Context))
		{
			break;
		}

		if (Context.IsFiniteContainer())
		{
			CborReader.SkipContainer(ECborCode::Array);
		}

		if (!CurrentKey.Equals(Key, ESearchCase::IgnoreCase))
		{
			continue;
		}

		switch (Context.MajorType())
		{
		case ECborCode::Int:
		case ECborCode::Uint:
		{
			if (SelectedDataType->Type != EMetadataFilterDataType::Int)
			{
				continue;
			}
			ensure(ConvertedData.IsType<int64>());

			int64 MetadataValue = static_cast<int64>(Context.AsUInt());
			int64 InputValue = ConvertedData.Get<int64>();

			FFilterOperator<int64>* Operator = (FFilterOperator<int64>*) SelectedOperator.Get();
			if (Operator->Apply(MetadataValue, InputValue))
			{
				return true;
			}
			
			continue;
		}

		case ECborCode::TextString:
		{
			if (SelectedDataType->Type != EMetadataFilterDataType::String)
			{
				continue;
			}

			FString Value = Context.AsString();
			FFilterOperator<FString>* Operator = (FFilterOperator<FString>*) SelectedOperator.Get();
			if (Operator->Apply(Value, Term))
			{
				return true;
			}

			continue;
		}

		case ECborCode::ByteString:
		{
			if (SelectedDataType->Type != EMetadataFilterDataType::String)
			{
				continue;
			}

			FAnsiStringView Value(Context.AsCString(), static_cast<int32>(Context.AsLength()));
			FString ValueStr(Value);

			FFilterOperator<FString>* Operator = (FFilterOperator<FString>*) SelectedOperator.Get();
			if (Operator->Apply(ValueStr, Term))
			{
				return true;
			}

			continue;
		}
		}

		if (Context.RawCode() == (ECborCode::Prim | ECborCode::Value_4Bytes))
		{
			if (SelectedDataType->Type != EMetadataFilterDataType::Double)
			{
				continue;
			}
			ensure(ConvertedData.IsType<double>());

			double Value = static_cast<double>(Context.AsFloat());
			double InputValue = ConvertedData.Get<double>();

			FFilterOperator<double>* Operator = (FFilterOperator<double>*) SelectedOperator.Get();
			if (Operator->Apply(Value, InputValue))
			{
				return true;
			}

			continue;
		}

		if (Context.RawCode() == (ECborCode::Prim | ECborCode::Value_8Bytes))
		{
			if (SelectedDataType->Type != EMetadataFilterDataType::Double)
			{
				continue;
			}
			ensure(ConvertedData.IsType<double>());

			double Value = Context.AsDouble();
			double InputValue = ConvertedData.Get<double>();

			FFilterOperator<double>* Operator = (FFilterOperator<double>*) SelectedOperator.Get();
			if (Operator->Apply(Value, InputValue))
			{
				return true;
			}

			continue;
		}

		if (Context.RawCode() == (ECborCode::Prim | ECborCode::False))
		{
			if (SelectedDataType->Type != EMetadataFilterDataType::Bool)
			{
				continue;
			}
			ensure(ConvertedData.IsType<bool>());

			FFilterOperator<bool>* Operator = (FFilterOperator<bool>*) SelectedOperator.Get();
			if (Operator->Apply(ConvertedData.Get<bool>(), false))
			{
				return true;
			}

			continue;
		}

		if (Context.RawCode() == (ECborCode::Prim | ECborCode::True))
		{
			if (SelectedDataType->Type != EMetadataFilterDataType::Bool)
			{
				continue;
			}
			ensure(ConvertedData.IsType<bool>());

			FFilterOperator<bool>* Operator = (FFilterOperator<bool>*) SelectedOperator.Get();
			if (Operator->Apply(ConvertedData.Get<bool>(), true))
			{
				return true;
			}

			continue;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMetadataFilterState::GetKeyTextBoxValue() const
{
	return FText::FromString(Key);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMetadataFilterState::OnKeyTextBoxValueCommitted(const FText& InNewText, ETextCommit::Type InTextCommit)
{
	Key = InNewText.ToString();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> FMetadataFilterState::DataType_OnGenerateWidget(TSharedPtr<FMetadataFilterDataTypeEntry> InDataType)
{
	TSharedRef<SHorizontalBox> Widget = SNew(SHorizontalBox);
	Widget->AddSlot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(InDataType->Name)
			.Margin(2.0f)
		];

	return Widget;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMetadataFilterState::DataType_OnSelectionChanged(TSharedPtr<FMetadataFilterDataTypeEntry> InDataType, ESelectInfo::Type SelectInfo)
{
	SelectedDataType = InDataType;

	AvailableOperators.Empty();
	SelectedOperator = nullptr;

	switch (SelectedDataType->Type)
	{
	case EMetadataFilterDataType::Bool:
	{
		AvailableOperators.Insert(BoolOperators, 0);
		break;
	}
	case EMetadataFilterDataType::Int:
	{
		AvailableOperators.Insert(*FFilterService::Get()->GetIntegerOperators(), 0);
		break;
	}
	case EMetadataFilterDataType::Double:
	{
		AvailableOperators.Insert(*FFilterService::Get()->GetDoubleOperators(), 0);
		break;
	}
	case EMetadataFilterDataType::String:
	{
		AvailableOperators.Insert(*FFilterService::Get()->GetStringOperators(), 0);
		break;
	}
	}

	if (AvailableOperators.Num() > 0)
	{
		AvailableOperators_OnSelectionChanged(AvailableOperators[0], ESelectInfo::Type::Direct);

		if (OperatorComboBox.IsValid())
		{
			OperatorComboBox->RefreshOptions();
			OperatorComboBox->SetSelectedItem(AvailableOperators[0]);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMetadataFilterState::DataType_GetSelectionText() const
{
	return SelectedDataType->Name;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> FMetadataFilterState::AvailableOperators_OnGenerateWidget(TSharedPtr<IFilterOperator> InOperator)
{
	TSharedRef<SHorizontalBox> Widget = SNew(SHorizontalBox);
	Widget->AddSlot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::FromString(InOperator->GetName()))
			.Margin(2.0f)
		];

	return Widget;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMetadataFilterState::AvailableOperators_OnSelectionChanged(TSharedPtr<IFilterOperator> InOperator, ESelectInfo::Type SelectInfo)
{
	if (InOperator)
	{
		SelectedOperator = InOperator;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMetadataFilterState::AvailableOperators_GetSelectionText() const
{
	return FText::FromString(SelectedOperator.IsValid() ? SelectedOperator->GetName(): TEXT(""));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMetadataFilterState::GetTermTextBoxValue() const
{
	return FText::FromString(Term);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMetadataFilterState::OnTermTextBoxValueCommitted(const FText& InNewText, ETextCommit::Type InTextCommit)
{
	Term = InNewText.ToString();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FMetadataFilterState::Equals(const FFilterState& Other) const
{
	if (this->GetTypeName() != Other.GetTypeName())
	{
		return false;
	}

	bool bIsEqual = true;

	const FMetadataFilterState& OtherMetadataFilter = StaticCast<const FMetadataFilterState&>(Other);
	
	bIsEqual &= Key.Equals(OtherMetadataFilter.Key);
	bIsEqual &= Term.Equals(OtherMetadataFilter.Term);
	bIsEqual &= SelectedDataType == OtherMetadataFilter.SelectedDataType;

	return bIsEqual;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<FFilterState> FMetadataFilterState::DeepCopy() const
{
	TSharedRef<FMetadataFilterState> Copy = MakeShared<FMetadataFilterState>(*this);
	
	return Copy;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FMetadataFilter
////////////////////////////////////////////////////////////////////////////////////////////////////

INSIGHTS_IMPLEMENT_RTTI(FMetadataFilter)

FMetadataFilter::FMetadataFilter()
	: FFilter(static_cast<int32>(EFilterField::Metadata),
		LOCTEXT("MetadataFilterName", "Metadata"),
		LOCTEXT("MetadataFilterDesc", "A filter for timing event metadata."),
		EFilterDataType::Custom,
		nullptr,
		nullptr)
{
	SupportedOperators = MakeShared<TArray<TSharedPtr<IFilterOperator>>>();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE