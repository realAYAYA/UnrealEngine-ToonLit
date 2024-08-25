// Copyright Epic Games, Inc. All Rights Reserved.

#include "Filters.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Docking/SDockTab.h"

// Insights
#include "Insights/InsightsStyle.h"
#include "Insights/ViewModels/FilterConfigurator.h"
#include "Insights/Widgets/SAdvancedFilter.h"

#define LOCTEXT_NAMESPACE "Insights::Filters"

namespace Insights
{

INSIGHTS_IMPLEMENT_RTTI(FFilterState)
INSIGHTS_IMPLEMENT_RTTI(FFilter)
INSIGHTS_IMPLEMENT_RTTI(FFilterWithSuggestions)
INSIGHTS_IMPLEMENT_RTTI(FCustomFilter)

////////////////////////////////////////////////////////////////////////////////////////////////////
// FFilterStorage
////////////////////////////////////////////////////////////////////////////////////////////////////

FFilterStorage::FFilterStorage()
{
	DoubleOperators = MakeShared<TArray<TSharedPtr<IFilterOperator>>>();
	DoubleOperators->Add(MakeShared<FFilterOperator<double>>(EFilterOperator::Eq, TEXT("="), std::equal_to<>()));
	DoubleOperators->Add(MakeShared<FFilterOperator<double>>(EFilterOperator::NotEq, TEXT("!="), std::not_equal_to<>()));
	DoubleOperators->Add(MakeShared<FFilterOperator<double>>(EFilterOperator::Lt, TEXT("<"), std::less<>{}));
	DoubleOperators->Add(MakeShared<FFilterOperator<double>>(EFilterOperator::Lte, TEXT("\u2264"), std::less_equal<>()));
	DoubleOperators->Add(MakeShared<FFilterOperator<double>>(EFilterOperator::Gt, TEXT(">"), std::greater<>()));
	DoubleOperators->Add(MakeShared<FFilterOperator<double>>(EFilterOperator::Gte, TEXT("\u2265"), std::greater_equal<>()));

	IntegerOperators = MakeShared<TArray<TSharedPtr<IFilterOperator>>>();
	IntegerOperators->Add(MakeShared<FFilterOperator<int64>>(EFilterOperator::Eq, TEXT("="), std::equal_to<>()));
	IntegerOperators->Add(MakeShared<FFilterOperator<int64>>(EFilterOperator::NotEq, TEXT("!="), std::not_equal_to<>()));
	IntegerOperators->Add(MakeShared<FFilterOperator<int64>>(EFilterOperator::Lt, TEXT("<"), std::less<>{}));
	IntegerOperators->Add(MakeShared<FFilterOperator<int64>>(EFilterOperator::Lte, TEXT("\u2264"), std::less_equal<>()));
	IntegerOperators->Add(MakeShared<FFilterOperator<int64>>(EFilterOperator::Eq, TEXT("="), std::equal_to<>()));
	IntegerOperators->Add(MakeShared<FFilterOperator<int64>>(EFilterOperator::Gt, TEXT(">"), std::greater<>()));
	IntegerOperators->Add(MakeShared<FFilterOperator<int64>>(EFilterOperator::Gte, TEXT("\u2265"), std::greater_equal<>()));

	StringOperators = MakeShared<TArray<TSharedPtr<IFilterOperator>>>();
	StringOperators->Add(MakeShared<FFilterOperator<FString>>(EFilterOperator::Eq, TEXT("IS"), [](const FString& lhs, const FString& rhs) { return lhs.Equals(rhs); }));
	StringOperators->Add(MakeShared<FFilterOperator<FString>>(EFilterOperator::NotEq, TEXT("IS NOT"), [](const FString& lhs, const FString& rhs) { return !lhs.Equals(rhs); }));
	StringOperators->Add(MakeShared<FFilterOperator<FString>>(EFilterOperator::Contains, TEXT("CONTAINS"), [](const FString& lhs, const FString& rhs) { return lhs.Contains(rhs); }));
	StringOperators->Add(MakeShared<FFilterOperator<FString>>(EFilterOperator::NotContains, TEXT("NOT CONTAINS"), [](const FString& lhs, const FString& rhs) { return !lhs.Contains(rhs); }));

	FilterGroupOperators.Add(MakeShared<FFilterGroupOperator>(EFilterGroupOperator::And, LOCTEXT("AllOf", "All Of (AND)"), LOCTEXT("AllOfDesc", "All of the children must be true for the group to return true. Equivalent to an AND operation.")));
	FilterGroupOperators.Add(MakeShared<FFilterGroupOperator>(EFilterGroupOperator::Or, LOCTEXT("AnyOf", "Any Of (OR)"), LOCTEXT("AnyOfDesc", "Any of the children must be true for the group to return true. Equivalent to an OR operation.")));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FFilterStorage::~FFilterStorage()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FFilterService
////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FFilterService> FFilterService::Instance;

FName const FFilterService::FilterConfiguratorTabId(TEXT("FilterConfigurator"));

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFilterService::Initialize()
{
	Instance = MakeShared<FFilterService>();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFilterService::Shutdown()
{
	Instance = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FFilterService::FFilterService()
{
	RegisterTabSpawner();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FFilterService::~FFilterService()
{
	UnregisterTabSpawner();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<SWidget> FFilterService::CreateFilterConfiguratorWidget(TSharedPtr<FFilterConfigurator> FilterConfiguratorViewModel)
{
	SAssignNew(PendingWidget, SAdvancedFilter, FilterConfiguratorViewModel);

	if (FGlobalTabmanager::Get()->HasTabSpawner(FilterConfiguratorTabId))
	{
		FGlobalTabmanager::Get()->TryInvokeTab(FilterConfiguratorTabId);
	}

	return PendingWidget;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SDockTab> FFilterService::SpawnTab(const FSpawnTabArgs& Args)
{
	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);

	const TSharedPtr<SWindow>& OwnerWindow = Args.GetOwnerWindow();
	if (OwnerWindow.IsValid())
	{
		const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);
		OwnerWindow->Resize(FVector2D(600 * DPIScaleFactor, 400 * DPIScaleFactor));
	}

	check(PendingWidget.IsValid());
	DockTab->SetContent(PendingWidget.ToSharedRef());
	PendingWidget->SetParentTab(DockTab);

	PendingWidget = nullptr;
	return DockTab;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFilterService::RegisterTabSpawner()
{
	FTabSpawnerEntry& TabSpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(FilterConfiguratorTabId,
		FOnSpawnTab::CreateRaw(this, &FFilterService::SpawnTab))
		.SetDisplayName(LOCTEXT("FilterConfiguratorTabTitle", "Filter Configurator"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.ClassicFilterConfig"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFilterService::UnregisterTabSpawner()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(FilterConfiguratorTabId);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFilterState::SetFilterValue(FString InTextValue)
{
	if (InTextValue.IsEmpty())
	{
		FilterValue = FFilterContext::ContextData();
	}

	switch (Filter->GetDataType())
	{
	case EFilterDataType::Double:
	{
		if (Filter->GetConverter().IsValid())
		{
			double Value = 0.0;
			FText Errors;
			bool Result = Filter->GetConverter()->Convert(InTextValue, Value, Errors);
			FilterValue.Set<double>(Result ? Value : 0.0);
		}
		else
		{
			FilterValue.Set<double>(FCString::Atod(*InTextValue));
		}
		break;
	}
	case EFilterDataType::Int64:
	{
		if (Filter->GetConverter().IsValid())
		{
			int64 Value = 0;
			FText Errors;
			bool Result = Filter->GetConverter()->Convert(InTextValue, Value, Errors);
			FilterValue.Set<int64>(Result ? Value : 0);
		}
		else
		{
			if (InTextValue.Contains(TEXT("x")))
			{
				FilterValue.Set<int64>((int64)FParse::HexNumber64(*InTextValue));
			}
			else
			{
				FilterValue.Set<int64>(FCString::Atoi64(*InTextValue));
			}
		}
		break;
	}
	case EFilterDataType::String:
	{
		FilterValue.Set<FString>(InTextValue);
		break;
	}
	case EFilterDataType::StringInt64Pair:
	{
		checkf(Filter->GetConverter().IsValid(), TEXT("StringToInt64Pair filters must have a converter set"));
		int64 Value = 0;
		FText Errors;
		bool Result = Filter->GetConverter()->Convert(InTextValue, Value, Errors);
		FilterValue.Set<int64>(Result ? Value : -1);
	}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FFilterState::ApplyFilter(const FFilterContext& Context) const
{
	if (!Context.HasFilterData(Filter->GetKey()))
	{
		// If data is not set for this filter return the value specified in the Context.
		return Context.GetReturnValueForUnsetFilters();
	}

	bool Ret = true;

	switch (Filter->GetDataType())
	{
	case EFilterDataType::Double:
	{
		FFilterOperator<double>* Operator = (FFilterOperator<double>*) SelectedOperator.Get();
		double Value = 0.0;
		Context.GetFilterData<double>(Filter->GetKey(), Value);

		Ret = Operator->Apply(Value, FilterValue.Get<double>());
		break;
	}
	case EFilterDataType::Int64:
	case EFilterDataType::StringInt64Pair:
	{
		FFilterOperator<int64>* Operator = (FFilterOperator<int64>*) SelectedOperator.Get();
		int64 Value = 0;
		Context.GetFilterData<int64>(Filter->GetKey(), Value);

		Ret = Operator->Apply(Value, FilterValue.Get<int64>());
		break;
	}
	case EFilterDataType::String:
	{
		FFilterOperator<FString>* Operator = (FFilterOperator<FString>*) SelectedOperator.Get();
		FString Value;
		Context.GetFilterData<FString>(Filter->GetKey(), Value);

		Ret = Operator->Apply(Value, FilterValue.Get<FString>());
		break;
	}
	default:
		break;
	}

	return Ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FFilterState::Equals(const FFilterState& Other) const
{
	return SelectedOperator == Other.GetSelectedOperator();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<FFilterState> FFilterState::DeepCopy() const
{
	return MakeShared<FFilterState>(*this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FFilterWithSuggestionsValueConverter::GetTooltipText() const
{
	return LOCTEXT("FilterWithSuggestionsValueConverterTooltip", "Enter the value to search for. This field has auto-complete. Start typing or press the arrow down or arrow up key to see options.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FFilterWithSuggestionsValueConverter::GetHintText() const
{
	return LOCTEXT("FilterWithSuggestionsValueConverterHint", "Start typing or press arrow down or up to see options.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE