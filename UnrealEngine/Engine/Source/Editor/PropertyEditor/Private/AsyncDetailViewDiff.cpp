// Copyright Epic Games, Inc. All Rights Reserved.
#include "AsyncDetailViewDiff.h"

#include "DetailTreeNode.h"
#include "IDetailsViewPrivate.h"
#include "DiffUtils.h"

static const UObject* GetObject(const TSharedPtr<FDetailTreeNode>& TreeNode)
{
	if (const IDetailsViewPrivate* DetailsView = TreeNode->GetDetailsView())
	{
		return DetailsView->GetSelectedObjects()[0].Get();
	}
	return nullptr;
}

static FResolvedProperty GetResolvedProperty(const TSharedPtr<FPropertyNode>& PropertyNode, const UObject* Object)
{
	if (PropertyNode)
	{
		const TSharedRef<FPropertyPath> PropertyPath = FPropertyNode::CreatePropertyPath(PropertyNode.ToSharedRef());
		if (PropertyPath->IsValid())
		{
			return FPropertySoftPath(*PropertyPath).Resolve(Object);
		}
	}
	return FResolvedProperty();
}

template<>
bool TreeDiffSpecification::AreValuesEqual<TWeakPtr<FDetailTreeNode>>(const TWeakPtr<FDetailTreeNode>& TreeNodeA,  const TWeakPtr<FDetailTreeNode>& TreeNodeB)
{
	const TSharedPtr<FDetailTreeNode> PinnedTreeNodeA = TreeNodeA.Pin();
	const TSharedPtr<FDetailTreeNode> PinnedTreeNodeB = TreeNodeB.Pin();
	if (!PinnedTreeNodeA || !PinnedTreeNodeB)
	{
		return PinnedTreeNodeA == PinnedTreeNodeB;
	}

	TArray<TSharedRef<FPropertyNode>> PropertyNodesA;
	TArray<TSharedRef<FPropertyNode>> PropertyNodesB;
	PinnedTreeNodeA->GetAllPropertyNodes(PropertyNodesA);
	PinnedTreeNodeB->GetAllPropertyNodes(PropertyNodesB);
	
	ensure(PropertyNodesA.Num() == PropertyNodesB.Num()); // AreMatching(...) should've stopped this from happening
	if (PropertyNodesA.IsEmpty() || PropertyNodesB.IsEmpty())
	{
		// category nodes
		return PinnedTreeNodeA->GetNodeName() == PinnedTreeNodeB->GetNodeName();
	}

	const UObject* OwningObjectA = GetObject(PinnedTreeNodeA);
	const UObject* OwningObjectB = GetObject(PinnedTreeNodeB);

	for (int32 PropNodeIndex = 0; PropNodeIndex < PropertyNodesA.Num(); ++PropNodeIndex)
	{
		const TSharedRef<FPropertyNode>& PropertyNodeA = PropertyNodesA[PropNodeIndex];
		const TSharedRef<FPropertyNode>& PropertyNodeB = PropertyNodesB[PropNodeIndex];

		const FResolvedProperty ResolvedA = GetResolvedProperty(PropertyNodeA, OwningObjectA);
		const FResolvedProperty ResolvedB = GetResolvedProperty(PropertyNodeB, OwningObjectB);
		if (ResolvedA.Property && ResolvedB.Property)
		{
			if (!ResolvedA.Property->SameType(ResolvedB.Property))
			{
				return false;
			}

			// use DiffUtils::Identical instead of FProperty::Identical so that sub-object pointers are considered identical
			// if they're isomorphic rather than only if they have the same pointer values
			if(!DiffUtils::Identical(ResolvedA, ResolvedB, OwningObjectA->GetPackage(), OwningObjectB->GetPackage()))
			{
				return false;
			}
		}
	}
	return true;
}

static bool MapKeysMatch(const TSharedRef<FPropertyNode>& MapPropertyNodeA, const TSharedRef<FPropertyNode>& MapPropertyNodeB, int32 KeyIndexA, int32 KeyIndexB,
	const UObject* OwningObjectA, const UObject* OwningObjectB)
{
	const FMapProperty* MapPropertyA = CastField<FMapProperty>(MapPropertyNodeA->GetProperty());
	const FMapProperty* MapPropertyB = CastField<FMapProperty>(MapPropertyNodeB->GetProperty());
	if (!MapPropertyA || !MapPropertyB)
	{
		return false;
	}
	
	const FResolvedProperty ResolvedMapA = GetResolvedProperty(MapPropertyNodeA, OwningObjectA);
	const FResolvedProperty ResolvedMapB = GetResolvedProperty(MapPropertyNodeB, OwningObjectB);
	FScriptMapHelper MapHelperA(MapPropertyA, MapPropertyA->ContainerPtrToValuePtr<UObject*>(ResolvedMapA.Object));
	FScriptMapHelper MapHelperB(MapPropertyB, MapPropertyB->ContainerPtrToValuePtr<UObject*>(ResolvedMapB.Object));
	
	const void* KeyA = MapHelperA.FindNthKeyPtr(KeyIndexA);
	const void* KeyB = MapHelperB.FindNthKeyPtr(KeyIndexB);
	
	if (MapPropertyA->KeyProp->SameType(MapPropertyB->KeyProp))
	{
		return MapPropertyA->KeyProp->Identical(KeyA, KeyB, PPF_DeepComparison);
	}
	return false;
}

static bool SetKeysMatch(const TSharedRef<FPropertyNode>& SetPropertyNodeA, const TSharedRef<FPropertyNode>& SetPropertyNodeB, int32 KeyIndexA, int32 KeyIndexB,
	const UObject* OwningObjectA, const UObject* OwningObjectB)
{
	const FSetProperty* SetPropertyA = CastField<FSetProperty>(SetPropertyNodeA->GetProperty());
	const FSetProperty* SetPropertyB = CastField<FSetProperty>(SetPropertyNodeB->GetProperty());
	if (!SetPropertyA || !SetPropertyB)
	{
		return false;
	}
	
	const FResolvedProperty ResolvedSetA = GetResolvedProperty(SetPropertyNodeA, OwningObjectA);
	const FResolvedProperty ResolvedSetB = GetResolvedProperty(SetPropertyNodeB, OwningObjectB);
	FScriptSetHelper SetHelperA(SetPropertyA, SetPropertyA->ContainerPtrToValuePtr<UObject*>(ResolvedSetA.Object));
	FScriptSetHelper SetHelperB(SetPropertyB, SetPropertyB->ContainerPtrToValuePtr<UObject*>(ResolvedSetB.Object));
	
	const void* KeyA = SetHelperA.FindNthElementPtr(KeyIndexA);
	const void* KeyB = SetHelperB.FindNthElementPtr(KeyIndexB);
	
	if (SetPropertyA->ElementProp->SameType(SetPropertyB->ElementProp))
	{
		return SetPropertyA->ElementProp->Identical(KeyA, KeyB, PPF_DeepComparison);
	}
	return false;
}

template<>
bool TreeDiffSpecification::AreMatching<TWeakPtr<FDetailTreeNode>>(const TWeakPtr<FDetailTreeNode>& TreeNodeA, const TWeakPtr<FDetailTreeNode>& TreeNodeB)
{
	const TSharedPtr<FDetailTreeNode> PinnedTreeNodeA = TreeNodeA.Pin();
	const TSharedPtr<FDetailTreeNode> PinnedTreeNodeB = TreeNodeB.Pin();
	if (!PinnedTreeNodeA || !PinnedTreeNodeB)
	{
		return PinnedTreeNodeA == PinnedTreeNodeB;
	}

	
	TArray<TSharedRef<FPropertyNode>> PropertyNodesA;
	TArray<TSharedRef<FPropertyNode>> PropertyNodesB;
	PinnedTreeNodeA->GetAllPropertyNodes(PropertyNodesA);
	PinnedTreeNodeB->GetAllPropertyNodes(PropertyNodesB);
	if (PropertyNodesA.Num() != PropertyNodesB.Num())
	{
		return false;
	}
	if (PropertyNodesA.IsEmpty())
	{
		// category nodes
		return PinnedTreeNodeA->GetNodeName() == PinnedTreeNodeB->GetNodeName();
	}
	
	const UObject* OwningObjectA = GetObject(PinnedTreeNodeA);
	const UObject* OwningObjectB = GetObject(PinnedTreeNodeB);
	
	for (int32 PropNodeIndex = 0; PropNodeIndex < PropertyNodesA.Num(); ++PropNodeIndex)
	{
		const TSharedRef<FPropertyNode>& PropertyNodeA = PropertyNodesA[PropNodeIndex];
		const TSharedRef<FPropertyNode>& PropertyNodeB = PropertyNodesB[PropNodeIndex];

		// property nodes
		const int32 ArrayIndexA = PropertyNodeA->GetArrayIndex();
		const int32 ArrayIndexB = PropertyNodeB->GetArrayIndex();
		const FProperty* PropertyA = PropertyNodeA->GetProperty();
		const FProperty* PropertyB = PropertyNodeB->GetProperty();
		
		if (ArrayIndexA != INDEX_NONE && ArrayIndexB != INDEX_NONE)
		{
			const TSharedRef<FPropertyNode> ParentPropertyNodeA = PropertyNodeA->GetParentNode()->AsShared();
			const TSharedRef<FPropertyNode> ParentPropertyNodeB = PropertyNodeB->GetParentNode()->AsShared();
			const FProperty* ParentPropertyA = ParentPropertyNodeA->GetProperty();
			const FProperty* ParentPropertyB = ParentPropertyNodeB->GetProperty();
			
			// sets and maps are stored by index in the property tree so we need to dig their keys out of the data
			// and compare those instead
			if (ParentPropertyA->IsA<FMapProperty>() || ParentPropertyB->IsA<FMapProperty>())
			{
				if (!MapKeysMatch(ParentPropertyNodeA, ParentPropertyNodeB, ArrayIndexA, ArrayIndexB, OwningObjectA, OwningObjectB))
				{
					return false;
				}
			}
			if (ParentPropertyA->IsA<FSetProperty>() || ParentPropertyB->IsA<FSetProperty>())
			{
				if (SetKeysMatch(ParentPropertyNodeA, ParentPropertyNodeB, ArrayIndexA, ArrayIndexB, OwningObjectA, OwningObjectB))
				{
					return false;
				}
			}
		}
		
		if (ArrayIndexA != ArrayIndexB)
		{
			return false;
		}
		const FName PropertyNameA = PropertyA ? PropertyA->GetFName() : NAME_None;
		const FName PropertyNameB = PropertyB ? PropertyB->GetFName() : NAME_None;

		if (PropertyNameA != PropertyNameB)
		{
			return false;
		}
	}
	return true;
}

template<>
void TreeDiffSpecification::GetChildren<TWeakPtr<FDetailTreeNode>>(const TWeakPtr<FDetailTreeNode>& InParent, TArray<TWeakPtr<FDetailTreeNode>>& OutChildren)
{
	const TSharedPtr<FDetailTreeNode> PinnedParent = InParent.Pin();
	if (PinnedParent)
	{
		TArray<TSharedRef<FDetailTreeNode>> Children;
		PinnedParent->GetChildren(Children);
		for (TSharedRef<FDetailTreeNode> Child : Children)
		{
			OutChildren.Add(Child);
		}
	}
}

template<>
bool TreeDiffSpecification::ShouldMatchByValue<TWeakPtr<FDetailTreeNode>>(const TWeakPtr<FDetailTreeNode>& TreeNode)
{
	const TSharedPtr<FDetailTreeNode> PinnedTreeNode = TreeNode.Pin();
	if (!PinnedTreeNode)
	{
		return false;
	}
	const TSharedPtr<FPropertyNode> PropertyNodeA = PinnedTreeNode->GetPropertyNode();
	if (!PropertyNodeA || !PropertyNodeA->GetParentNode())
	{
		return false;
	}
	
	const int32 ArrayIndex = PropertyNodeA->GetArrayIndex();
	const FArrayProperty* ParentArrayProperty = CastField<FArrayProperty>(PropertyNodeA->GetParentNode()->GetProperty());
	
	// match array elements by value rather than by index
	return ParentArrayProperty && ArrayIndex != INDEX_NONE;
}

FAsyncDetailViewDiff::FAsyncDetailViewDiff(TSharedRef<IDetailsView> InLeftView, TSharedRef<IDetailsView> InRightView)
	: TAsyncTreeDifferences(RootNodesAttribute(InLeftView), RootNodesAttribute(InRightView))
	, LeftView(InLeftView)
	, RightView(InRightView)
{}

void FAsyncDetailViewDiff::GetPropertyDifferences(TArray<FSingleObjectDiffEntry>& OutDiffEntries) const
{
	ForEach(ETreeTraverseOrder::PreOrder, [&](const TUniquePtr<DiffNodeType>& Node)->ETreeTraverseControl
	{
		FPropertyPath PropertyPath;
		FPropertyPath RightPropertyPath;
		if (const TSharedPtr<FDetailTreeNode> LeftTreeNode = Node->ValueA.Pin())
		{
			PropertyPath = LeftTreeNode->GetPropertyPath();
		}
		else if (const TSharedPtr<FDetailTreeNode> RightTreeNode = Node->ValueB.Pin())
		{
			PropertyPath = RightTreeNode->GetPropertyPath();
		}

		// only include tree nodes with properties
		if (!PropertyPath.IsValid())
		{
			return ETreeTraverseControl::Continue;
		}
		
		EPropertyDiffType::Type PropertyDiffType;
        switch(Node->DiffResult)
        {
        case ETreeDiffResult::MissingFromTree1: 
            PropertyDiffType = EPropertyDiffType::PropertyAddedToB;
            break;
        case ETreeDiffResult::MissingFromTree2: 
            PropertyDiffType = EPropertyDiffType::PropertyAddedToA;
            break;
        case ETreeDiffResult::DifferentValues: 
            PropertyDiffType = EPropertyDiffType::PropertyValueChanged;
            break;
        default:
        	// only include changes
            return ETreeTraverseControl::Continue;
        }

		OutDiffEntries.Add(FSingleObjectDiffEntry(PropertyPath, PropertyDiffType));
        return ETreeTraverseControl::SkipChildren; // only include top-most properties
	});
}

TPair<int32, int32> FAsyncDetailViewDiff::ForEachRow(const TFunction<ETreeTraverseControl(const TUniquePtr<DiffNodeType>&, int32, int32)>& Method) const
{
	const TSharedPtr<IDetailsView> LeftDetailsView = LeftView.Pin();
	const TSharedPtr<IDetailsView> RightDetailsView = RightView.Pin();
	if (!LeftDetailsView || !RightDetailsView)
	{
		return {0,0};
	}
	
	int32 LeftRowNum = 0;
	int32 RightRowNum = 0;
	ForEach(
		ETreeTraverseOrder::PreOrder,
		[&LeftDetailsView,&RightDetailsView,&Method,&LeftRowNum,&RightRowNum](const TUniquePtr<DiffNodeType>& DiffNode)->ETreeTraverseControl
		{
			bool bFoundLeftRow = false;
			if (const TSharedPtr<FDetailTreeNode> LeftTreeNode = DiffNode->ValueA.Pin())
			{
				if (!LeftDetailsView->IsAncestorCollapsed(LeftTreeNode.ToSharedRef()))
				{
					bFoundLeftRow = true;
				}
			}
			
			bool bFoundRightRow = false;
			if (const TSharedPtr<FDetailTreeNode> RightTreeNode = DiffNode->ValueB.Pin())
			{
				if (!RightDetailsView->IsAncestorCollapsed(RightTreeNode.ToSharedRef()))
				{
					bFoundRightRow = true;
				}
			}

			ETreeTraverseControl Control = ETreeTraverseControl::SkipChildren;
			if (bFoundRightRow || bFoundLeftRow)
			{
				Control = Method(DiffNode, LeftRowNum, RightRowNum);
			}
			
			if (bFoundLeftRow)
			{
				++LeftRowNum;
			}
			if (bFoundRightRow)
			{
				++RightRowNum;
			}
			
			return Control;
		}
	);
	return {LeftRowNum, RightRowNum};
}

TAttribute<TArray<TWeakPtr<FDetailTreeNode>>> FAsyncDetailViewDiff::RootNodesAttribute(TWeakPtr<IDetailsView> DetailsView)
{
	return TAttribute<TArray<TWeakPtr<FDetailTreeNode>>>::CreateLambda([DetailsView]()
	{
		TArray<TWeakPtr<FDetailTreeNode>> Result;
		if (const TSharedPtr<IDetailsViewPrivate> Details = StaticCastSharedPtr<IDetailsViewPrivate>(DetailsView.Pin()))
		{
			Details->GetHeadNodes(Result);
		}
		return Result;
	});
}

