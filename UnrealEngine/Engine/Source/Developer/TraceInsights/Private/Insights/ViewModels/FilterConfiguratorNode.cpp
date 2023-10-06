// Copyright Epic Games, Inc. All Rights Reserved.

#include "FilterConfiguratorNode.h"

#include "Misc/Parse.h"

// Insights
#include "Insights/ViewModels/Filters.h"

#define LOCTEXT_NAMESPACE "Insights::FFilterConfiguratorNode"

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

TSharedRef<FFilterConfiguratorNode> FFilterConfiguratorNode::DeepCopy(const FFilterConfiguratorNode& InSourceNode)
{
	TSharedRef<FFilterConfiguratorNode> NodeCopyPtr = MakeShared<FFilterConfiguratorNode>(InSourceNode.GetName(), InSourceNode.IsGroup());
	FFilterConfiguratorNode& NodeCopy = *NodeCopyPtr;

	NodeCopy.AvailableFilters = InSourceNode.AvailableFilters;
	NodeCopy.AvailableFilterOperators = InSourceNode.AvailableFilterOperators;
	NodeCopy.SelectedFilter = InSourceNode.SelectedFilter;
	NodeCopy.SelectedFilterOperator = InSourceNode.SelectedFilterOperator;
	NodeCopy.SelectedFilterGroupOperator = InSourceNode.SelectedFilterGroupOperator;
	NodeCopy.TextBoxValue = InSourceNode.TextBoxValue;

	NodeCopy.SetExpansion(InSourceNode.IsExpanded());

	if (InSourceNode.IsGroup())
	{
		check(NodeCopy.IsGroup());
		NodeCopy.ClearChildren(InSourceNode.GetChildrenCount());
		for (FBaseTreeNodePtr Child : InSourceNode.GetChildren())
		{
			check(Child.IsValid() && Child->Is<FFilterConfiguratorNode>());
			TSharedPtr<FFilterConfiguratorNode> ChildCopy = FFilterConfiguratorNode::DeepCopy(Child->As<FFilterConfiguratorNode>());
			NodeCopy.AddChildAndSetParent(ChildCopy);
		}
	}

	return NodeCopyPtr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FFilterConfiguratorNode::operator==(const FFilterConfiguratorNode& Other) const
{
	bool bIsEqual = true;
	bIsEqual &= AvailableFilters.Get() == Other.AvailableFilters.Get();
	bIsEqual &= SelectedFilter.Get() == Other.SelectedFilter.Get();
	bIsEqual &= SelectedFilterOperator.Get() == Other.SelectedFilterOperator.Get();
	bIsEqual &= SelectedFilterGroupOperator.Get() == Other.SelectedFilterGroupOperator.Get();
	bIsEqual &= TextBoxValue == Other.TextBoxValue;
	bIsEqual &= GetChildrenCount() == Other.GetChildrenCount();

	if (bIsEqual)
	{
		const int32 Count = GetChildrenCount();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			bIsEqual &= *StaticCastSharedPtr<FFilterConfiguratorNode>(GetChildren()[Index]) == *StaticCastSharedPtr<FFilterConfiguratorNode>(Other.GetChildren()[Index]);
		}
	}

	return bIsEqual;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TArray<TSharedPtr<FFilterGroupOperator>>& FFilterConfiguratorNode::GetFilterGroupOperators()
{
	return FFilterService::Get()->GetFilterGroupOperators();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFilterConfiguratorNode::SetAvailableFilters(TSharedPtr<TArray<TSharedPtr<FFilter>>> InAvailableFilters)
{
	AvailableFilters = InAvailableFilters;

	if (AvailableFilters.IsValid() && AvailableFilters->Num() > 0)
	{
		SetSelectedFilter(AvailableFilters->GetData()[0]);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFilterConfiguratorNode::SetSelectedFilter(TSharedPtr<FFilter> InSelectedFilter)
{
	SelectedFilter = InSelectedFilter;
	if (SelectedFilter.IsValid() && SelectedFilter->GetSupportedOperators()->Num() > 0)
	{
		SetSelectedFilterOperator(SelectedFilter->GetSupportedOperators()->GetData()[0]);

		AvailableFilterOperators->Empty();
		SupportedOperatorsArrayPtr AvailableOperators = InSelectedFilter->GetSupportedOperators();
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
		TArray<FBaseTreeNodePtr> ChildArray = GetChildrenMutable();
		for (FBaseTreeNodePtr Child : ChildArray)
		{
			FFilterConfiguratorNodePtr CastedChild = StaticCastSharedPtr<FFilterConfiguratorNode>(Child);
			CastedChild->ProcessFilter();
		}
	}
	else
	{
		switch (SelectedFilter->GetDataType())
		{
		case EFilterDataType::Double:
		{
			if (SelectedFilter->GetConverter().IsValid())
			{
				double Value = 0.0;
				FText Errors;
				bool Result = SelectedFilter->GetConverter()->Convert(TextBoxValue, Value, Errors);
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
			if (SelectedFilter->GetConverter().IsValid())
			{
				int64 Value = 0;
				FText Errors;
				bool Result = SelectedFilter->GetConverter()->Convert(TextBoxValue, Value, Errors);
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
			checkf(SelectedFilter->GetConverter().IsValid(), TEXT("StringToInt64Pair filters must have a converter set"));
			int64 Value = 0;
			FText Errors;
			bool Result = SelectedFilter->GetConverter()->Convert(TextBoxValue, Value, Errors);
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
		KeysUsed.Add(SelectedFilter->GetKey());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FFilterConfiguratorNode::ApplyFilters(const FFilterContext& Context) const
{
	bool Ret = true;
	if (IsGroup())
	{
		switch (SelectedFilterGroupOperator->GetType())
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
		if (!Context.HasFilterData(SelectedFilter->GetKey()))
		{
			// If data is not set for this filter return the value specified in the Context.
			return Context.GetReturnValueForUnsetFilters();
		}

		switch (SelectedFilter->GetDataType())
		{
		case EFilterDataType::Double:
		{
			FFilterOperator<double>* Operator = (FFilterOperator<double>*) SelectedFilterOperator.Get();
			double Value = 0.0;
			Context.GetFilterData<double>(SelectedFilter->GetKey(), Value);

			Ret = Operator->Apply(Value, FilterValue.Get<double>());
			break;
		}
		case EFilterDataType::Int64:
		case EFilterDataType::StringInt64Pair:
		{
			FFilterOperator<int64>* Operator = (FFilterOperator<int64>*) SelectedFilterOperator.Get();
			int64 Value = 0;
			Context.GetFilterData<int64>(SelectedFilter->GetKey(), Value);

			Ret = Operator->Apply(Value, FilterValue.Get<int64>());
			break;
		}
		case EFilterDataType::String:
		{
			FFilterOperator<FString>* Operator = (FFilterOperator<FString>*) SelectedFilterOperator.Get();
			FString Value;
			Context.GetFilterData<FString>(SelectedFilter->GetKey(), Value);

			Ret = Operator->Apply(Value, FilterValue.Get<FString>());
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
