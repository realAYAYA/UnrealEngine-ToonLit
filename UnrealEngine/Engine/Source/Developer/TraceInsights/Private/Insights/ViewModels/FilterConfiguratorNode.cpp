// Copyright Epic Games, Inc. All Rights Reserved.

#include "FilterConfiguratorNode.h"

#include "Misc/Parse.h"

// Insights
#include "Insights/ViewModels/Filters.h"

#define LOCTEXT_NAMESPACE "FFilterConfiguratorNode"

namespace Insights
{

INSIGHTS_IMPLEMENT_RTTI(FFilterConfiguratorNode)

////////////////////////////////////////////////////////////////////////////////////////////////////

FFilterConfiguratorNode::FFilterConfiguratorNode(const FName InName, bool bInIsGroup)
	: FBaseTreeNode(InName, bInIsGroup)
{
	if (bInIsGroup)
	{
		SelectedFilterGroupOperator = GetFilterGroupOperators()[0];
	}

	AvailableFilterOperators = MakeShared<TArray<TSharedPtr<IFilterOperator>>>();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FFilterConfiguratorNode::FFilterConfiguratorNode(const FFilterConfiguratorNode& Other)
	: FBaseTreeNode(Other.GetName(), Other.IsGroup())
{
	*this = Other;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FFilterConfiguratorNode& FFilterConfiguratorNode::operator=(const FFilterConfiguratorNode& Other)
{
	GetChildrenMutable().Empty();

	AvailableFilters = Other.AvailableFilters;
	AvailableFilterOperators = Other.AvailableFilterOperators;
	SelectedFilter = Other.SelectedFilter;
	SelectedFilterOperator = Other.SelectedFilterOperator;
	SelectedFilterGroupOperator = Other.SelectedFilterGroupOperator;
	TextBoxValue = Other.TextBoxValue;
	SetExpansion(Other.IsExpanded());

	for (Insights::FBaseTreeNodePtr Child : Other.GetChildren())
	{
		GetChildrenMutable().Add(MakeShared<FFilterConfiguratorNode>(*StaticCastSharedPtr<FFilterConfiguratorNode>(Child)));
	}

	return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FFilterConfiguratorNode::operator==(const FFilterConfiguratorNode& Other)
{
	bool bIsEqual = true;
	bIsEqual &= AvailableFilters.Get() == Other.AvailableFilters.Get();
	bIsEqual &= SelectedFilter.Get() == Other.SelectedFilter.Get();
	bIsEqual &= SelectedFilterOperator.Get() == Other.SelectedFilterOperator.Get();
	bIsEqual &= SelectedFilterGroupOperator.Get() == Other.SelectedFilterGroupOperator.Get();
	bIsEqual &= TextBoxValue == Other.TextBoxValue;
	bIsEqual &= GetChildren().Num() == Other.GetChildren().Num();

	if (bIsEqual)
	{
		for (int32 Index = 0; Index < GetChildren().Num(); ++Index)
		{
			bIsEqual &= *StaticCastSharedPtr<FFilterConfiguratorNode>(GetChildren()[Index]) == *StaticCastSharedPtr<FFilterConfiguratorNode>(Other.GetChildren()[Index]);
		}
	}

	return bIsEqual;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TArray<TSharedPtr<struct FFilterGroupOperator>>& FFilterConfiguratorNode::GetFilterGroupOperators()
{
	return FFilterService::Get()->GetFilterGroupOperators();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFilterConfiguratorNode::DeleteChildNode(FFilterConfiguratorNodePtr InNode)
{
	Insights::FBaseTreeNodePtr Node = StaticCastSharedPtr<Insights::FBaseTreeNode>(InNode);
	GetChildrenMutable().Remove(Node);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFilterConfiguratorNode::SetGroupPtrForChildren()
{
	for (Insights::FBaseTreeNodePtr Child : GetChildrenMutable())
	{
		FFilterConfiguratorNodePtr CastedChild = StaticCastSharedPtr<FFilterConfiguratorNode>(Child);
		CastedChild->SetGroupPtrForChildren();
		CastedChild->SetGroupPtr(SharedThis(this));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFilterConfiguratorNode::SetAvailableFilters(TSharedPtr<TArray<TSharedPtr<struct FFilter>>> InAvailableFilters)
{
	AvailableFilters = InAvailableFilters;

	if (AvailableFilters.IsValid() && AvailableFilters->Num() > 0)
	{
		SetSelectedFilter(AvailableFilters->GetData()[0]);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFilterConfiguratorNode::SetSelectedFilter(TSharedPtr<struct FFilter> InSelectedFilter)
{
	SelectedFilter = InSelectedFilter;
	if (SelectedFilter.IsValid() && SelectedFilter->GetSupportedOperators()->Num() > 0)
	{
		SetSelectedFilterOperator(SelectedFilter->GetSupportedOperators()->GetData()[0]);

		AvailableFilterOperators->Empty();
		Insights::SupportedOperatorsArrayPtr AvailableOperators = InSelectedFilter->SupportedOperators;
		for (auto& FilterOperator : *AvailableOperators)
		{
			AvailableFilterOperators->Add(FilterOperator);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFilterConfiguratorNode::ProcessFilter()
{
	if (IsGroup())
	{
		TArray<Insights::FBaseTreeNodePtr> ChildArray = GetChildrenMutable();
		for (Insights::FBaseTreeNodePtr Child : ChildArray)
		{
			FFilterConfiguratorNodePtr CastedChild = StaticCastSharedPtr<FFilterConfiguratorNode>(Child);
			CastedChild->ProcessFilter();
		}
	}
	else
	{
		switch (SelectedFilter->DataType)
		{
		case EFilterDataType::Double:
		{
			if (SelectedFilter->Converter.IsValid())
			{
				double Value = 0.0;
				FText Errors;
				bool Result = SelectedFilter->Converter->Convert(TextBoxValue, Value, Errors);
				FilterValue.Set<double>(Result ? Value : 0.0);
			}
			else
			{
				FilterValue.Set<double>(FCString::Atod(*TextBoxValue));
			}
			break;
		}
		case EFilterDataType::Int64:
		{
			if (SelectedFilter->Converter.IsValid())
			{
				int64 Value = 0;
				FText Errors;
				bool Result = SelectedFilter->Converter->Convert(TextBoxValue, Value, Errors);
				FilterValue.Set<int64>(Result ? Value : 0);
			}
			else
			{
				if (TextBoxValue.Contains(TEXT("x")))
				{
					FilterValue.Set<int64>((int64)FParse::HexNumber64(*TextBoxValue));
				}
				else
				{
					FilterValue.Set<int64>(FCString::Atoi64(*TextBoxValue));
				}
			}
			break;
		}
		case EFilterDataType::String:
		{
			FilterValue.Set<FString>(TextBoxValue);
			break;
		}
		case EFilterDataType::StringInt64Pair:
		{
			checkf(SelectedFilter->Converter.IsValid(), TEXT("StringToInt64Pair filters must have a converter set"));
			int64 Value = 0;
			FText Errors;
			bool Result = SelectedFilter->Converter->Convert(TextBoxValue, Value, Errors);
			FilterValue.Set<int64>(Result ? Value : -1);
		}
		}
	}
}

void FFilterConfiguratorNode::GetUsedKeys(TSet<int32>& KeysUsed) const
{
	if (IsGroup())
	{
		auto& ChildrenArr = GetChildren();
		for (int Index = 0; Index < ChildrenArr.Num(); ++Index)
		{
			((FFilterConfiguratorNode*)ChildrenArr[Index].Get())->GetUsedKeys(KeysUsed);
		}
	}
	else
	{
		KeysUsed.Add(SelectedFilter->Key);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FFilterConfiguratorNode::ApplyFilters(const FFilterContext& Context) const
{
	bool Ret = true;
	if (IsGroup())
	{
		switch (SelectedFilterGroupOperator->Type)
		{
		case EFilterGroupOperator::And:
		{
			auto& ChildrenArr = GetChildren();
			for (int Index = 0; Index < ChildrenArr.Num() && Ret; ++Index)
			{
				Ret &= ((FFilterConfiguratorNode*)ChildrenArr[Index].Get())->ApplyFilters(Context);
			}
			break;
		}
		case EFilterGroupOperator::Or:
		{
			auto& ChildrenArr = GetChildren();
			if (ChildrenArr.Num() > 0)
			{
				Ret = false;
			}

			for (int Index = 0; Index < ChildrenArr.Num() && !Ret; ++Index)
			{
				Ret |= ((FFilterConfiguratorNode*)ChildrenArr[Index].Get())->ApplyFilters(Context);
			}
			break;
		}
		}
	}
	else
	{
		if (!Context.HasFilterData(SelectedFilter->Key))
		{
			// If data is not set for this filter return the value specified in the Context.
			return Context.GetReturnValueForUnsetFilters();
		}

		switch (SelectedFilter->DataType)
		{
		case EFilterDataType::Double:
		{
			FFilterOperator<double>* Operator = (FFilterOperator<double>*) SelectedFilterOperator.Get();
			double Value = 0.0;
			Context.GetFilterData<double>(SelectedFilter->Key, Value);

			Ret = Operator->Func(Value, FilterValue.Get<double>());
			break;
		}
		case EFilterDataType::Int64:
		case EFilterDataType::StringInt64Pair:
		{
			FFilterOperator<int64>* Operator = (FFilterOperator<int64>*) SelectedFilterOperator.Get();
			int64 Value = 0;
			Context.GetFilterData<int64>(SelectedFilter->Key, Value);

			Ret = Operator->Func(Value, FilterValue.Get<int64>());
			break;
		}
		case EFilterDataType::String:
		{
			FFilterOperator<FString>* Operator = (FFilterOperator<FString>*) SelectedFilterOperator.Get();
			FString Value;
			Context.GetFilterData<FString>(SelectedFilter->Key, Value);

			Ret = Operator->Func(Value, FilterValue.Get<FString>());
			break;
		}
		default:
			break;
		}
	}

	return Ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE
