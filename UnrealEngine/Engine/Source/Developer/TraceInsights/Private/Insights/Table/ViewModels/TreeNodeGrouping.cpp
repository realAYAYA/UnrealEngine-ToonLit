// Copyright Epic Games, Inc. All Rights Reserved.

#include "TreeNodeGrouping.h"

#include "Insights/Common/AsyncOperationProgress.h"
#include "Insights/Table/ViewModels/TableTreeNode.h"

#define LOCTEXT_NAMESPACE "Insights::FTreeNodeGrouping"

namespace Insights
{

INSIGHTS_IMPLEMENT_RTTI(ITreeNodeGrouping)
INSIGHTS_IMPLEMENT_RTTI(FTreeNodeGrouping)
INSIGHTS_IMPLEMENT_RTTI(FTreeNodeGroupingFlat)
INSIGHTS_IMPLEMENT_RTTI(FTreeNodeGroupingByUniqueValue)
INSIGHTS_IMPLEMENT_RTTI(FTreeNodeGroupingByNameFirstLetter)
INSIGHTS_IMPLEMENT_RTTI(FTreeNodeGroupingByType)
INSIGHTS_IMPLEMENT_RTTI(FTreeNodeGroupingByPathBreakdown)

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTreeNodeGrouping
////////////////////////////////////////////////////////////////////////////////////////////////////

FTreeNodeGrouping::FTreeNodeGrouping(const FText& InShortName, const FText& InTitleName, const FText& InDescription, const FName InBrushName, const FSlateBrush* InIcon)
	: ShortName(InShortName)
	, TitleName(InTitleName)
	, Description(InDescription)
	, BrushName(InBrushName)
	, Icon(InIcon)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTreeNodeGrouping::GroupNodes(const TArray<FTableTreeNodePtr>& Nodes, FTableTreeNode& ParentGroup, TWeakPtr<FTable> InParentTable, IAsyncOperationProgress& InAsyncOperationProgress) const
{
	TMap<FName, FTableTreeNodePtr> GroupMap;

	ParentGroup.ClearChildren();

	for (FTableTreeNodePtr NodePtr : Nodes)
	{
		if (InAsyncOperationProgress.ShouldCancelAsyncOp())
		{
			return;
		}

		if (NodePtr->IsGroup())
		{
			ParentGroup.AddChildAndSetParent(NodePtr);
			continue;
		}

		FTableTreeNodePtr GroupPtr = nullptr;

		FTreeNodeGroupInfo GroupInfo = GetGroupForNode(NodePtr);
		FTableTreeNodePtr* GroupPtrPtr = GroupMap.Find(GroupInfo.Name);
		if (!GroupPtrPtr)
		{
			GroupPtr = MakeShared<FTableTreeNode>(GroupInfo.Name, InParentTable);
			GroupPtr->SetExpansion(GroupInfo.IsExpanded);
			ParentGroup.AddChildAndSetParent(GroupPtr);
			GroupMap.Add(GroupInfo.Name, GroupPtr);
		}
		else
		{
			GroupPtr = *GroupPtrPtr;
		}

		GroupPtr->AddChildAndSetParent(NodePtr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTreeNodeGroupingFlat
////////////////////////////////////////////////////////////////////////////////////////////////////

FTreeNodeGroupingFlat::FTreeNodeGroupingFlat()
	: FTreeNodeGrouping(
		LOCTEXT("Grouping_Flat_ShortName", "All"),
		LOCTEXT("Grouping_Flat_TitleName", "Flat (All)"),
		LOCTEXT("Grouping_Flat_Desc", "Creates a single group. Includes all items."),
		TEXT("Icons.Group.TreeItem"),
		nullptr)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTreeNodeGroupingFlat::GroupNodes(const TArray<FTableTreeNodePtr>& Nodes, FTableTreeNode& ParentGroup, TWeakPtr<FTable> InParentTable, IAsyncOperationProgress& InAsyncOperationProgress) const
{
	ParentGroup.ClearChildren(1);

	FTableTreeNodePtr GroupPtr = MakeShared<FTableTreeNode>(FName(TEXT("All")), InParentTable);
	GroupPtr->SetExpansion(true);
	ParentGroup.AddChildAndSetParent(GroupPtr);

	GroupPtr->ClearChildren(Nodes.Num());
	for (FTableTreeNodePtr NodePtr : Nodes)
	{
		if (InAsyncOperationProgress.ShouldCancelAsyncOp())
		{
			return;
		}

		GroupPtr->AddChildAndSetParent(NodePtr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTreeNodeGroupingByUniqueValue
////////////////////////////////////////////////////////////////////////////////////////////////////

FTreeNodeGroupingByUniqueValue::FTreeNodeGroupingByUniqueValue(TSharedRef<FTableColumn> InColumnRef)
	: FTreeNodeGrouping(
		FText::Format(LOCTEXT("Grouping_ByUniqueValue_ShortNameFmt", "{0} (Unique)"), InColumnRef->GetTitleName()),
		FText::Format(LOCTEXT("Grouping_ByUniqueValue_TitleNameFmt", "Unique Values - {0}"), InColumnRef->GetTitleName()),
		LOCTEXT("Grouping_ByUniqueValue_Desc", "Creates a group for each unique value."),
		TEXT("Icons.Group.TreeItem"),
		nullptr)
	, ColumnRef(InColumnRef)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTreeNodeGroupInfo FTreeNodeGroupingByUniqueValue::GetGroupForNode(const FBaseTreeNodePtr InNode) const
{
	FTableTreeNodePtr TableTreeNodePtr = StaticCastSharedPtr<FTableTreeNode>(InNode);
	FText ValueAsText = ColumnRef->GetValueAsText(*TableTreeNodePtr);
	FStringView GroupName(ValueAsText.ToString());
	if (GroupName.Len() >= NAME_SIZE)
	{
		GroupName = FStringView(GroupName.GetData(), NAME_SIZE - 1);
	}
	return { FName(GroupName, 0), false };
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// TTreeNodeGroupingByUniqueValue specializations
////////////////////////////////////////////////////////////////////////////////////////////////////

template<> bool TTreeNodeGroupingByUniqueValue<bool>::GetValue(const FTableCellValue& CellValue) { return CellValue.Bool; }
template<> int64 TTreeNodeGroupingByUniqueValue<int64>::GetValue(const FTableCellValue& CellValue) { return CellValue.Int64; }
template<> float TTreeNodeGroupingByUniqueValue<float>::GetValue(const FTableCellValue& CellValue) { return CellValue.Float; }
template<> double TTreeNodeGroupingByUniqueValue<double>::GetValue(const FTableCellValue& CellValue) { return CellValue.Double; }

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTreeNodeGroupingByUniqueValueCString
////////////////////////////////////////////////////////////////////////////////////////////////////

FName FTreeNodeGroupingByUniqueValueCString::GetGroupName(const TCHAR* Value)
{
	FStringView StringView(Value);
	if (StringView.Len() >= NAME_SIZE)
	{
		StringView = FStringView(StringView.GetData(), NAME_SIZE - 1);
	}
	return FName(StringView, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTreeNodeGroupingByUniqueValueCString::GroupNodes(const TArray<FTableTreeNodePtr>& Nodes, FTableTreeNode& ParentGroup, TWeakPtr<FTable> InParentTable, IAsyncOperationProgress& InAsyncOperationProgress) const
{
	FTableTreeNodePtr UnsetGroupPtr = nullptr; // for unset FTableCellValue
	FTableTreeNodePtr EmptyGroupPtr = nullptr; // for nullptr and empty strings
	TMap<const TCHAR*, FTableTreeNodePtr, FDefaultSetAllocator, TStringPointerMapKeyFuncs_DEPRECATED<const TCHAR*, FTableTreeNodePtr>> GroupMap; // for valid strings
	TMap<FName, FTableTreeNodePtr> GroupNameMap; // for valid strings

	ParentGroup.ClearChildren();

	for (FTableTreeNodePtr NodePtr : Nodes)
	{
		if (InAsyncOperationProgress.ShouldCancelAsyncOp())
		{
			return;
		}

		if (NodePtr->IsGroup())
		{
			ParentGroup.AddChildAndSetParent(NodePtr);
			continue;
		}

		FTableTreeNodePtr GroupPtr = nullptr;

		const FTableColumn& Column = *GetColumn();
		const TOptional<FTableCellValue> CellValue = Column.GetValue(*NodePtr);
		if (CellValue.IsSet())
		{
			const TCHAR* Value = CellValue.GetValue().CString;
			if (Value != nullptr && Value[0] != TEXT('\0'))
			{
				FTableTreeNodePtr* GroupPtrPtr = GroupMap.Find(Value);
				if (!GroupPtrPtr)
				{
					const FName GroupName = GetGroupName(Value);
					GroupPtrPtr = GroupNameMap.Find(Value);
					if (!GroupPtrPtr)
					{
						GroupPtr = MakeShared<FTableTreeNode>(GroupName, InParentTable);
						GroupPtr->SetExpansion(false);
						ParentGroup.AddChildAndSetParent(GroupPtr);
						GroupNameMap.Add(GroupName, GroupPtr);
					}
					else
					{
						GroupPtr = *GroupPtrPtr;
					}
					GroupMap.Add(Value, GroupPtr);
				}
				else
				{
					GroupPtr = *GroupPtrPtr;
				}
			}
			else
			{
				if (!EmptyGroupPtr)
				{
					EmptyGroupPtr = MakeShared<FTableTreeNode>(FName(TEXT("N/A")), InParentTable);
					EmptyGroupPtr->SetExpansion(false);
					ParentGroup.AddChildAndSetParent(EmptyGroupPtr);
				}
				GroupPtr = EmptyGroupPtr;
			}
		}
		else
		{
			if (!UnsetGroupPtr)
			{
				UnsetGroupPtr = MakeShared<FTableTreeNode>(FName(TEXT("<unset>")), InParentTable);
				UnsetGroupPtr->SetExpansion(false);
				ParentGroup.AddChildAndSetParent(UnsetGroupPtr);
			}
			GroupPtr = UnsetGroupPtr;
		}

		GroupPtr->AddChildAndSetParent(NodePtr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTreeNodeGroupingByNameFirstLetter
////////////////////////////////////////////////////////////////////////////////////////////////////

FTreeNodeGroupingByNameFirstLetter::FTreeNodeGroupingByNameFirstLetter()
	: FTreeNodeGrouping(
		LOCTEXT("Grouping_ByName_ShortName", "Name"),
		LOCTEXT("Grouping_ByName_TitleName", "Name (First Letter)"),
		LOCTEXT("Grouping_ByName_Desc", "Creates a group for each first letter of node names."),
		TEXT("Icons.Group.TreeItem"),
		nullptr)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTreeNodeGroupInfo FTreeNodeGroupingByNameFirstLetter::GetGroupForNode(const FBaseTreeNodePtr InNode) const
{
	return { *InNode->GetName().GetPlainNameString().Left(1), false };
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTreeNodeGroupingByType
////////////////////////////////////////////////////////////////////////////////////////////////////

FTreeNodeGroupingByType::FTreeNodeGroupingByType()
	: FTreeNodeGrouping(
		LOCTEXT("Grouping_ByTypeName_ShortName", "TypeName"),
		LOCTEXT("Grouping_ByTypeName_TitleName", "Type Name"),
		LOCTEXT("Grouping_ByTypeName_Desc", "Creates a group for each node type."),
		TEXT("Icons.Group.TreeItem"),
		nullptr)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTreeNodeGroupInfo FTreeNodeGroupingByType::GetGroupForNode(const FBaseTreeNodePtr InNode) const
{
	return { InNode->GetTypeName(), true };
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTreeNodeGroupingByPathBreakdown
////////////////////////////////////////////////////////////////////////////////////////////////////

FTreeNodeGroupingByPathBreakdown::FTreeNodeGroupingByPathBreakdown(TSharedRef<FTableColumn> InColumnRef)
	: FTreeNodeGrouping(
		FText::Format(LOCTEXT("Grouping_ByPathBreakdown_ShortNameFmt", "{0} (Path)"), InColumnRef->GetTitleName()),
		FText::Format(LOCTEXT("Grouping_ByPathBreakdown_TitleNameFmt", "Path Breakdown - {0}"), InColumnRef->GetTitleName()),
		LOCTEXT("Grouping_ByPathBreakdown_Desc", "Creates a tree hierarchy out of the path structure of string values."),
		TEXT("Icons.Group.TreeItem"),
		nullptr)
	, ColumnRef(InColumnRef)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTreeNodeGroupingByPathBreakdown::GroupNodes(
	const TArray<FTableTreeNodePtr>& Nodes,
	FTableTreeNode& ParentGroup,
	TWeakPtr<FTable> InParentTable,
	IAsyncOperationProgress& InAsyncOperationProgress) const
{
	ParentGroup.ClearChildren();

	struct FDirectory
	{
		FDirectory() = delete;
		FDirectory(FName InName, FDirectory* InParent, FTableTreeNode* InNode)
			: Name(InName)
			, Parent(InParent)
			, Node(InNode)
		{
		}

		FName Name;
		FDirectory* Parent;
		FTableTreeNode* Node;
		TMap<FName, FDirectory*> ChildrenMap; // Directory Name -> FDirectory*
	};

	TArray<FDirectory*> Directories;

	static FName RootName(TEXT("~"));
	FDirectory* Root = new FDirectory(RootName, nullptr, &ParentGroup);
	Directories.Add(Root);

	FTableTreeNode* UnsetGroupPtr = nullptr;
	TMap<FString, FDirectory*> FullNameGroupMap; // FullName -> FDirectory*

	for (FTableTreeNodePtr NodePtr : Nodes)
	{
		if (InAsyncOperationProgress.ShouldCancelAsyncOp())
		{
			return;
		}

		if (NodePtr->IsGroup())
		{
			ParentGroup.AddChildAndSetParent(NodePtr);
			continue;
		}

		FString FullName;
		const TOptional<FTableCellValue> OptionalValue = ColumnRef->GetValue(*NodePtr);
		if (OptionalValue.IsSet())
		{
			FullName = OptionalValue.GetValue().AsString();
		}

		FDirectory* Directory = Root;

		if (!FullName.IsEmpty())
		{
			FDirectory** FoundDir = FullNameGroupMap.Find(FullName);
			if (FoundDir)
			{
				Directory = *FoundDir;
			}
			else
			{
				const TCHAR* Start = *FullName;
				const TCHAR* End = Start;
				while (true)
				{
					if (*End == TEXT('/') || *End == TEXT('\\') || *End == TEXT('\0'))
					{
						const FName DirName(int32(End - Start), Start, 0);

						FoundDir = Directory->ChildrenMap.Find(DirName);
						if (FoundDir)
						{
							Directory = *FoundDir;
						}
						else if (!DirName.IsNone())
						{
							FTableTreeNodePtr GroupPtr = MakeShared<FTableTreeNode>(DirName, InParentTable);
							GroupPtr->SetExpansion(false);
							Directory->Node->AddChildAndSetParent(GroupPtr);
							Directory = new FDirectory(DirName, Directory, GroupPtr.Get());
							Directory->Parent->ChildrenMap.Add(DirName, Directory);
							Directories.Add(Directory);
						}

						if (*End == TEXT('\0'))
						{
							FullNameGroupMap.Add(FullName, Directory);
							break;
						}

						Start = End + 1;
					}
					++End;
				}
			}
		}

		if (Directory != Root)
		{
			Directory->Node->AddChildAndSetParent(NodePtr);
		}
		else
		{
			if (!UnsetGroupPtr)
			{
				static FName NotAvailableName(TEXT("N/A"));
				FTableTreeNodePtr NotAvailableNode = MakeShared<FTableTreeNode>(NotAvailableName, InParentTable);
				NotAvailableNode->SetExpansion(false);
				ParentGroup.AddChildAndSetParent(NotAvailableNode);
				UnsetGroupPtr = NotAvailableNode.Get();
			}
			UnsetGroupPtr->AddChildAndSetParent(NodePtr);
		}
	}

	for (FDirectory* Dir : Directories)
	{
		delete Dir;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE
