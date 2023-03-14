// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXPixelMappingEditorUtils.h"

#include "DMXPixelMapping.h"
#include "DMXPixelMappingComponentReference.h"
#include "DMXPixelMappingEditorCommon.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "Components/DMXPixelMappingRootComponent.h"
#include "DragDrop/DMXPixelMappingDragDropOp.h"
#include "Toolkits/DMXPixelMappingToolkit.h"

#include "ScopedTransaction.h"

#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Input/DragAndDrop.h"
#include "Layout/ArrangedWidget.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "FDMXPixelMappingEditorUtils"


bool FDMXPixelMappingEditorUtils::VerifyComponentRename(TSharedRef<FDMXPixelMappingToolkit> InToolkit, const FDMXPixelMappingComponentReference& InComponent, const FText& NewName, FText& OutErrorMessage)
{
	if (!InComponent.IsValid())
	{
		OutErrorMessage = LOCTEXT("InvalidComponentReference", "Invalid Component Reference");
		return false;
	}

	if (NewName.IsEmptyOrWhitespace())
	{
		OutErrorMessage = LOCTEXT("EmptyComponentName", "Empty Component Name");
		return false;
	}

	const FString& NewNameString = NewName.ToString();

	if (NewNameString.Len() >= NAME_SIZE)
	{
		OutErrorMessage = LOCTEXT("ComponentNameTooLong", "Component Name is Too Long");
		return false;
	}

	UDMXPixelMappingBaseComponent* ComponentToRename = InComponent.GetComponent();
	if (!ComponentToRename)
	{
		// In certain situations, the template might be lost due to mid recompile with focus lost on the rename box at
		// during a strange moment.
		return false;
	}

	// Slug the new name down to a valid object name
	const FName NewNameSlug = MakeObjectNameFromDisplayLabel(NewNameString, ComponentToRename->GetFName());

	UDMXPixelMapping* DMXPixelMapping = InToolkit->GetDMXPixelMapping();
	UDMXPixelMappingBaseComponent* ExistingComponent = DMXPixelMapping->FindComponent(NewNameSlug);

	if (ExistingComponent != nullptr)
	{
		if (ComponentToRename != ExistingComponent)
		{
			OutErrorMessage = LOCTEXT("ExistingComponentName", "Existing Component Name");
			return false;
		}
	}
	else
	{
		// check for redirectors too
		if (FindObject<UObject>(ComponentToRename->GetOuter(), *NewNameSlug.ToString()))
		{
			OutErrorMessage = LOCTEXT("ExistingOldComponentName", "Existing Old Component Name");
			return false;
		}
	}

	return true;
}

void FDMXPixelMappingEditorUtils::RenameComponent(TSharedRef<FDMXPixelMappingToolkit> InToolkit, const FName& OldObjectName, const FString& NewDisplayName)
{
	UDMXPixelMapping* DMXPixelMapping = InToolkit->GetDMXPixelMapping();
	check(DMXPixelMapping);

	UDMXPixelMappingBaseComponent* ComponentToRename = DMXPixelMapping->FindComponent(OldObjectName);
	check(ComponentToRename);

	// Get the new FName slug from the given display name
	const FName NewFName = MakeObjectNameFromDisplayLabel(NewDisplayName, ComponentToRename->GetFName());
	const FString NewNameStr = NewFName.ToString();

	ComponentToRename->Rename(*NewNameStr);

	InToolkit->OnComponentRenamed(ComponentToRename);
}

UDMXPixelMappingRendererComponent* FDMXPixelMappingEditorUtils::AddRenderer(UDMXPixelMapping* InPixelMapping)
{
	if (InPixelMapping == nullptr)
	{
		UE_LOG(LogDMXPixelMappingEditor, Warning, TEXT("Cannot find PixelMapping in AddRenderer call. PixelMapping is nullptr."));

		return nullptr;
	}	
	
	if (InPixelMapping->RootComponent == nullptr)
	{
		UE_LOG(LogDMXPixelMappingEditor, Warning, TEXT("Cannot find RootComponent in AddRenderer call. RootComponent is nullptr."));

		return nullptr;
	}

	// Get root componet
	UDMXPixelMappingBaseComponent* RootComponent = InPixelMapping->RootComponent;

	// Create renderer name
	UDMXPixelMappingBaseComponent* DefaultComponent = UDMXPixelMappingRendererComponent::StaticClass()->GetDefaultObject<UDMXPixelMappingRendererComponent>();
	FName UniqueName = MakeUniqueObjectName(RootComponent, UDMXPixelMappingRendererComponent::StaticClass(), FName(TEXT("OutputMapping")));

	// Create new renderer and add to Root
	UDMXPixelMappingRendererComponent* Component = NewObject<UDMXPixelMappingRendererComponent>(RootComponent, UDMXPixelMappingRendererComponent::StaticClass(), UniqueName, RF_Transactional);
	RootComponent->AddChild(Component);

	return Component;
}

void FDMXPixelMappingEditorUtils::CreateComponentContextMenu(FMenuBuilder& MenuBuilder, TSharedRef<FDMXPixelMappingToolkit> InToolkit)
{
	MenuBuilder.BeginSection("Edit", LOCTEXT("Edit", "Edit"));
	{
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Cut);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Copy);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Paste);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Duplicate);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
	}
	MenuBuilder.EndSection();
}


bool FDMXPixelMappingEditorUtils::GetArrangedWidget(TSharedRef<SWidget> InWidget, FArrangedWidget& OutArrangedWidget)
{
	TSharedPtr<SWindow> WidgetWindow = FSlateApplication::Get().FindWidgetWindow(InWidget);
	if (!WidgetWindow.IsValid())
	{
		return false;
	}

	TSharedRef<SWindow> CurrentWindowRef = WidgetWindow.ToSharedRef();

	FWidgetPath WidgetPath;
	if (FSlateApplication::Get().GeneratePathToWidgetUnchecked(InWidget, WidgetPath))
	{
		OutArrangedWidget = WidgetPath.FindArrangedWidget(InWidget).Get(FArrangedWidget::GetNullWidget());
		return true;
	}

	return false;
}

UDMXPixelMappingBaseComponent* FDMXPixelMappingEditorUtils::GetTargetComponentFromDragDropEvent(const TWeakPtr<FDMXPixelMappingToolkit>& WeakToolkit, const FDragDropEvent& DragDropEvent)
{
	if (TSharedPtr<FDMXPixelMappingToolkit> ToolkitPtr = WeakToolkit.Pin())
	{
		TSharedPtr<FDMXPixelMappingDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FDMXPixelMappingDragDropOp>();
		if (DragDropOp.IsValid())
		{
			UDMXPixelMappingBaseComponent* Target = (DragDropOp->Parent.IsValid()) ? DragDropOp->Parent.Get() : ToolkitPtr->GetActiveRendererComponent();

			return Target;
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
