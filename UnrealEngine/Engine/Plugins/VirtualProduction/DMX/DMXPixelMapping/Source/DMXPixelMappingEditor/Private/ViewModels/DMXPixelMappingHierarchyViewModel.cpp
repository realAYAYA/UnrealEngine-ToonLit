// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/DMXPixelMappingHierarchyViewModel.h"
#include "Toolkits/DMXPixelMappingToolkit.h"
#include "DMXPixelMapping.h"
#include "Components/DMXPixelMappingRootComponent.h"
#include "DMXPixelMappingComponentReference.h"
#include "DMXPixelMappingEditorUtils.h"

#define LOCTEXT_NAMESPACE "DMXPixelMappingHierarchyItemWidgetModel"

FDMXPixelMappingHierarchyItemWidgetModel::FDMXPixelMappingHierarchyItemWidgetModel(FDMXPixelMappingToolkitPtr InToolkit)
	: ToolkitWeakPtr(InToolkit)
	, bInitialized(false)
	, bIsSelected(false)
{
	UDMXPixelMapping* DMXPixelMapping = InToolkit->GetDMXPixelMapping();

	if (DMXPixelMapping != nullptr && DMXPixelMapping->RootComponent != nullptr)
	{
		Reference = InToolkit->GetReferenceFromComponent(DMXPixelMapping->RootComponent);
	}
}

FDMXPixelMappingHierarchyItemWidgetModel::FDMXPixelMappingHierarchyItemWidgetModel(FDMXPixelMappingComponentReference InReference, FDMXPixelMappingToolkitPtr InToolkit)
	: ToolkitWeakPtr(InToolkit)
	, Reference(InReference)
	, bInitialized(false)
	, bIsSelected(false)
{
	check(Reference.GetComponent() != nullptr);
}


FText FDMXPixelMappingHierarchyItemWidgetModel::GetText() const
{
	if (UDMXPixelMappingBaseComponent* Component = Reference.GetComponent())
	{
		return FText::FromString(Component->GetUserFriendlyName());
	}

	return FText::GetEmpty();
}

bool FDMXPixelMappingHierarchyItemWidgetModel::OnVerifyNameTextChanged(const FText& InText, FText& OutErrorMessage)
{
	return FDMXPixelMappingEditorUtils::VerifyComponentRename(ToolkitWeakPtr.Pin().ToSharedRef(), Reference, InText, OutErrorMessage);
}

void FDMXPixelMappingHierarchyItemWidgetModel::OnNameTextCommited(const FText& InText, ETextCommit::Type CommitInfo)
{
	FDMXPixelMappingEditorUtils::RenameComponent(ToolkitWeakPtr.Pin().ToSharedRef(), Reference.GetComponent()->GetFName(), InText.ToString());
}

void FDMXPixelMappingHierarchyItemWidgetModel::OnSelection()
{
	SelectedComponentReferences.Add(Reference);

	if (FDMXPixelMappingToolkitPtr ToolkitPtr = ToolkitWeakPtr.Pin())
	{
		ToolkitPtr->SelectComponents(SelectedComponentReferences);
	}
}

void FDMXPixelMappingHierarchyItemWidgetModel::RequestBeginRename()
{
	RenameEvent.ExecuteIfBound();
}

void FDMXPixelMappingHierarchyItemWidgetModel::GetChildren(FDMXPixelMappingHierarchyItemWidgetModelArr& Children)
{
	if (FDMXPixelMappingToolkitPtr ToolkitPtr = ToolkitWeakPtr.Pin())
	{
		if (UDMXPixelMappingBaseComponent* Component = Reference.GetComponent())
		{
			for (int32 ChildIndex = 0; ChildIndex < Component->GetChildrenCount(); ChildIndex++)
			{
				UDMXPixelMappingBaseComponent* Child = Component->GetChildAt(ChildIndex);
				if (Child)
				{
					FDMXPixelMappingHierarchyItemWidgetModelPtr ChildItem = MakeShared<FDMXPixelMappingHierarchyItemWidgetModel>(ToolkitPtr->GetReferenceFromComponent(Child), ToolkitPtr);
					Children.Add(ChildItem);
				}
			}
		}
	}
}

void FDMXPixelMappingHierarchyItemWidgetModel::UpdateSelection()
{
	bIsSelected = false;
	const TSet<FDMXPixelMappingComponentReference>& SelectedComponents = ToolkitWeakPtr.Pin()->GetSelectedComponents();
	for (const FDMXPixelMappingComponentReference& SelectedComponent : SelectedComponents)
	{
		if (SelectedComponent.GetComponent() == Reference.GetComponent())
		{
			bIsSelected = true;
			return;
		}
	}
}


void FDMXPixelMappingHierarchyItemWidgetModel::GatherChildren(FDMXPixelMappingHierarchyItemWidgetModelArr& Children)
{
	InitializeChildren();

	Children.Append(Models);
}

void FDMXPixelMappingHierarchyItemWidgetModel::RefreshSelection()
{
	InitializeChildren();

	UpdateSelection();

	for (FDMXPixelMappingHierarchyItemWidgetModelPtr& Model : Models)
	{
		Model->RefreshSelection();
	}
}

bool FDMXPixelMappingHierarchyItemWidgetModel::ContainsSelection()
{
	InitializeChildren();

	for (FDMXPixelMappingHierarchyItemWidgetModelPtr& Model : Models)
	{
		if (Model->IsSelected() || Model->ContainsSelection())
		{
			return true;
		}
	}

	return false;
}


void FDMXPixelMappingHierarchyItemWidgetModel::InitializeChildren()
{
	if (!bInitialized)
	{
		bInitialized = true;
		GetChildren(Models);
	}
}

#undef LOCTEXT_NAMESPACE