// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackPropertyRow.h"
#include "NiagaraNode.h"

#include "IDetailTreeNode.h"
#include "PropertyHandle.h"
#include "IDetailPropertyRow.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraStackPropertyRow)

void UNiagaraStackPropertyRow::Initialize(FRequiredEntryData InRequiredEntryData, TSharedRef<IDetailTreeNode> InDetailTreeNode, bool bInIsTopLevelProperty, FString InOwnerStackItemEditorDataKey, FString InOwnerStackEditorDataKey, UNiagaraNode* InOwningNiagaraNode)
{
	TSharedPtr<IPropertyHandle> PropertyHandle = InDetailTreeNode->CreatePropertyHandle();
	FString RowStackEditorDataKey = FString::Printf(TEXT("%s-%s"), *InOwnerStackEditorDataKey, *InDetailTreeNode->GetNodeName().ToString());
	Super::Initialize(InRequiredEntryData, InOwnerStackItemEditorDataKey, RowStackEditorDataKey);
	bool bRowIsAdvanced = PropertyHandle.IsValid() && PropertyHandle->GetProperty() && PropertyHandle->GetProperty()->HasAnyPropertyFlags(CPF_AdvancedDisplay);
	SetIsAdvanced(bRowIsAdvanced);
	DetailTreeNode = InDetailTreeNode;
	bIsTopLevelProperty = bInIsTopLevelProperty;
	OwningNiagaraNode = InOwningNiagaraNode;
	CategorySpacer = nullptr;
	if (DetailTreeNode->GetNodeType() == EDetailNodeType::Category)
	{
		RowStyle = bInIsTopLevelProperty ? EStackRowStyle::ItemCategory : EStackRowStyle::ItemSubCategory;
	}
	else
	{
		RowStyle = EStackRowStyle::ItemContent;
	}
	bCannotEditInThisContext = false;
	if (PropertyHandle.IsValid() && PropertyHandle.Get() && PropertyHandle->GetProperty())
	{
		FProperty* Prop = PropertyHandle->GetProperty();
		FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop);
		if (ObjProp && ObjProp->PropertyClass && (ObjProp->PropertyClass->IsChildOf(AActor::StaticClass()) || ObjProp->PropertyClass->IsChildOf(UActorComponent::StaticClass())))
		{
			bCannotEditInThisContext = true;
		}
	}
}

TSharedRef<IDetailTreeNode> UNiagaraStackPropertyRow::GetDetailTreeNode() const
{
	return DetailTreeNode.ToSharedRef();
}

bool UNiagaraStackPropertyRow::GetIsEnabled() const
{
	if (bCannotEditInThisContext) 
		return false;
	return OwningNiagaraNode == nullptr || OwningNiagaraNode->GetDesiredEnabledState() == ENodeEnabledState::Enabled;
}

UNiagaraStackEntry::EStackRowStyle UNiagaraStackPropertyRow::GetStackRowStyle() const
{
	return RowStyle;
}

bool UNiagaraStackPropertyRow::HasOverridenContent() const
{
	TSharedPtr<IPropertyHandle> PropertyHandle = DetailTreeNode->CreatePropertyHandle();
	if (PropertyHandle.IsValid() && PropertyHandle.Get())
	{
		return PropertyHandle->DiffersFromDefault();
	}
	return false;
}

bool UNiagaraStackPropertyRow::IsExpandedByDefault() const
{
	return DetailTreeNode->GetInitiallyCollapsed() == false;
}

bool UNiagaraStackPropertyRow::CanDrag() const
{
	TSharedPtr<IPropertyHandle> PropertyHandle = DetailTreeNode->CreatePropertyHandle();
	return PropertyHandle.IsValid() && PropertyHandle->GetParentHandle().IsValid() && PropertyHandle->GetParentHandle()->AsArray().IsValid();
}

void UNiagaraStackPropertyRow::FinalizeInternal()
{
	Super::FinalizeInternal();
	DetailTreeNode.Reset();
}

void UNiagaraStackPropertyRow::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	TArray<TSharedRef<IDetailTreeNode>> NodeChildren;
	DetailTreeNode->GetChildren(NodeChildren);
	for (TSharedRef<IDetailTreeNode> NodeChild : NodeChildren)
	{
		if (NodeChild->GetNodeType() == EDetailNodeType::Advanced)
		{
			continue;
		}

		UNiagaraStackPropertyRow* ChildRow = FindCurrentChildOfTypeByPredicate<UNiagaraStackPropertyRow>(CurrentChildren,
			[=](UNiagaraStackPropertyRow* CurrentChild) { return CurrentChild->GetDetailTreeNode() == NodeChild; });

		if (ChildRow == nullptr)
		{
			bool bChildIsTopLevelProperty = false;
			ChildRow = NewObject<UNiagaraStackPropertyRow>(this);
			ChildRow->Initialize(CreateDefaultChildRequiredData(), NodeChild, bChildIsTopLevelProperty, GetOwnerStackItemEditorDataKey(), GetStackEditorDataKey(), OwningNiagaraNode);
		}

		NewChildren.Add(ChildRow);
	}

	if (bIsTopLevelProperty && DetailTreeNode->GetNodeType() == EDetailNodeType::Category)
	{
		if (CategorySpacer == nullptr)
		{
			CategorySpacer = NewObject<UNiagaraStackSpacer>(this);
			TAttribute<bool> ShouldShowSpacerInStack;
			ShouldShowSpacerInStack.BindUObject(this, &UNiagaraStackPropertyRow::GetShouldShowInStack);
			CategorySpacer->Initialize(CreateDefaultChildRequiredData(), 6, ShouldShowSpacerInStack, GetStackEditorDataKey());
		}
		NewChildren.Add(CategorySpacer);
	}
}

int32 UNiagaraStackPropertyRow::GetChildIndentLevel() const
{
	// We want to keep inputs under a top level category at the same indent level as the category.
	return bIsTopLevelProperty && DetailTreeNode->GetNodeType() == EDetailNodeType::Category ? GetIndentLevel() : Super::GetChildIndentLevel();
}

void UNiagaraStackPropertyRow::GetSearchItems(TArray<FStackSearchItem>& SearchItems) const
{
	SearchItems.Add({ FName("DisplayName"), GetDisplayName() });

	TArray<FString> NodeFilterStrings;
	DetailTreeNode->GetFilterStrings(NodeFilterStrings);
	for (FString& FilterString : NodeFilterStrings)
	{
		SearchItems.Add({ "PropertyRowFilterString", FText::FromString(FilterString) });
	}

	TSharedPtr<IDetailPropertyRow> DetailPropertyRow = DetailTreeNode->GetRow();
	if (DetailPropertyRow.IsValid())
	{
		TSharedPtr<IPropertyHandle> PropertyHandle = DetailPropertyRow->GetPropertyHandle();
		if (PropertyHandle)
		{
			FText PropertyRowHandleText;
			PropertyHandle->GetValueAsDisplayText(PropertyRowHandleText);
			SearchItems.Add({ "PropertyRowHandleText", PropertyRowHandleText });
		}
	}	
}

TOptional<UNiagaraStackEntry::FDropRequestResponse> UNiagaraStackPropertyRow::CanDropInternal(const FDropRequest& DropRequest)
{
	// Validate stack, drop zone, and drag type.
	if (DropRequest.DropOptions == UNiagaraStackEntry::EDropOptions::Overview ||
		DropRequest.DropZone == EItemDropZone::OntoItem ||
		DropRequest.DragDropOperation->IsOfType<FNiagaraStackEntryDragDropOp>() == false)
	{
		return TOptional<FDropRequestResponse>();
	}

	// Validate stack entry count and type.
	TSharedRef<const FNiagaraStackEntryDragDropOp> StackEntryDragDropOp = StaticCastSharedRef<const FNiagaraStackEntryDragDropOp>(DropRequest.DragDropOperation);
	if (StackEntryDragDropOp->GetDraggedEntries().Num() != 1 || StackEntryDragDropOp->GetDraggedEntries()[0]->IsA<UNiagaraStackPropertyRow>() == false)
	{
		return TOptional<FDropRequestResponse>();
	}

	auto HaveSameParent = [](TSharedPtr<IPropertyHandle> HandleA, TSharedPtr<IPropertyHandle> HandleB)
	{
		TSharedPtr<IPropertyHandle> ParentA = HandleA->GetParentHandle();
		TSharedPtr<IPropertyHandle> ParentB = HandleB->GetParentHandle();
		if (ParentA.IsValid() && ParentB.IsValid() && ParentA->GetProperty() == ParentB->GetProperty())
		{
			TArray<UObject*> OuterObjectsA;
			ParentA->GetOuterObjects(OuterObjectsA);
			TArray<UObject*> OuterObjectsB;
			ParentB->GetOuterObjects(OuterObjectsB);
			return OuterObjectsA == OuterObjectsB;
		}
		return false;
	};

	// Validate property handle.
	UNiagaraStackPropertyRow* DraggedPropertyRow = CastChecked<UNiagaraStackPropertyRow>(StackEntryDragDropOp->GetDraggedEntries()[0]);
	TSharedPtr<IPropertyHandle> DraggedPropertyHandle = DraggedPropertyRow->GetDetailTreeNode()->CreatePropertyHandle();
	TSharedPtr<IPropertyHandle> TargetPropertyHandle = DetailTreeNode->CreatePropertyHandle();
	if (DraggedPropertyHandle.IsValid() == false ||
		TargetPropertyHandle.IsValid() == false ||
		DraggedPropertyHandle == TargetPropertyHandle ||
		DraggedPropertyHandle->GetParentHandle().IsValid() == false ||
		HaveSameParent(DraggedPropertyHandle, TargetPropertyHandle) == false ||
		DraggedPropertyHandle->GetParentHandle()->AsArray().IsValid() == false)
	{
		return TOptional<FDropRequestResponse>();
	}
	return FDropRequestResponse(DropRequest.DropZone, NSLOCTEXT("NiagaraStackPropertyRow", "DropArrayItemMessage", "Move this array entry here."));
}

TOptional<UNiagaraStackEntry::FDropRequestResponse> UNiagaraStackPropertyRow::DropInternal(const FDropRequest& DropRequest)
{
	TOptional<FDropRequestResponse> CanDropResponse = CanDropInternal(DropRequest);
	if (CanDropResponse.IsSet())
	{
		TSharedRef<const FNiagaraStackEntryDragDropOp> StackEntryDragDropOp = StaticCastSharedRef<const FNiagaraStackEntryDragDropOp>(DropRequest.DragDropOperation);
		UNiagaraStackPropertyRow* DraggedPropertyRow = CastChecked<UNiagaraStackPropertyRow>(StackEntryDragDropOp->GetDraggedEntries()[0]);
		TSharedPtr<IPropertyHandle> DraggedPropertyHandle = DraggedPropertyRow->GetDetailTreeNode()->CreatePropertyHandle();
		TSharedPtr<IPropertyHandle> TargetPropertyHandle = DetailTreeNode->CreatePropertyHandle();
		int32 IndexOffset = DropRequest.DropZone == EItemDropZone::AboveItem ? 0 : 1;
		TargetPropertyHandle->GetParentHandle()->AsArray()->MoveElementTo(DraggedPropertyHandle->GetIndexInArray(), TargetPropertyHandle->GetIndexInArray() + IndexOffset);
	}
	return CanDropResponse;
}

