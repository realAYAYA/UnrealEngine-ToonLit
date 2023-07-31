// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/ControlRigContextMenuContext.h"

#include "ControlRigBlueprint.h"
#include "Editor/ControlRigEditor.h"
#include "Framework/Application/SlateApplication.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigContextMenuContext)

FString FControlRigRigHierarchyToGraphDragAndDropContext::GetSectionTitle() const
{
	TArray<FString> ElementNameStrings;
	for (const FRigElementKey& Element: DraggedElementKeys)
	{
		ElementNameStrings.Add(Element.Name.ToString());
	}
	FString SectionTitle = FString::Join(ElementNameStrings, TEXT(","));
	if(SectionTitle.Len() > 64)
	{
		SectionTitle = SectionTitle.Left(61) + TEXT("...");
	}
	return SectionTitle;
}

void UControlRigContextMenuContext::Init(TWeakPtr<FControlRigEditor> InControlRigEditor, const FControlRigMenuSpecificContext& InMenuSpecificContext)
{
	WeakControlRigEditor = InControlRigEditor;
	MenuSpecificContext = InMenuSpecificContext;
}

UControlRigBlueprint* UControlRigContextMenuContext::GetControlRigBlueprint() const
{
	if (const TSharedPtr<FControlRigEditor> Editor = WeakControlRigEditor.Pin())
	{
		return Editor->GetControlRigBlueprint();
	}
	
	return nullptr;
}

UControlRig* UControlRigContextMenuContext::GetControlRig() const
{
	if (UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint())
	{
		if (UControlRig* ControlRig = Cast<UControlRig>(RigBlueprint->GetObjectBeingDebugged()))
		{
			return ControlRig;
		}
	}
	return nullptr;
}

bool UControlRigContextMenuContext::IsAltDown() const
{
	return FSlateApplication::Get().GetModifierKeys().IsAltDown();
}

FControlRigRigHierarchyDragAndDropContext UControlRigContextMenuContext::GetRigHierarchyDragAndDropContext()
{
	return MenuSpecificContext.RigHierarchyDragAndDropContext;
}

FControlRigGraphNodeContextMenuContext UControlRigContextMenuContext::GetGraphNodeContextMenuContext()
{
	return MenuSpecificContext.GraphNodeContextMenuContext;
}

FControlRigRigHierarchyToGraphDragAndDropContext UControlRigContextMenuContext::GetRigHierarchyToGraphDragAndDropContext()
{
	return MenuSpecificContext.RigHierarchyToGraphDragAndDropContext;
}

SRigHierarchy* UControlRigContextMenuContext::GetRigHierarchyPanel() const
{
	if (const TSharedPtr<SRigHierarchy> RigHierarchyPanel = MenuSpecificContext.RigHierarchyPanel.Pin())
	{
		return RigHierarchyPanel.Get();
	}
	return nullptr;
}

FControlRigEditor* UControlRigContextMenuContext::GetControlRigEditor() const
{
	if (const TSharedPtr<FControlRigEditor> Editor = WeakControlRigEditor.Pin())
	{
		return Editor.Get();
	}
	return nullptr;
}

