// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/SDMXPixelMappingDetailsView.h"

#include "Toolkits/DMXPixelMappingToolkit.h"
#include "DMXPixelMappingComponentReference.h"
#include "Components/DMXPixelMappingRootComponent.h"
#include "Components/DMXPixelMappingFixtureGroupComponent.h"
#include "Components/DMXPixelMappingFixtureGroupItemComponent.h"
#include "Components/DMXPixelMappingScreenComponent.h"
#include "Components/DMXPixelMappingMatrixComponent.h"
#include "Components/DMXPixelMappingMatrixCellComponent.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "Customizations/DMXPixelMappingDetailCustomization_FixtureGroup.h"
#include "Customizations/DMXPixelMappingDetailCustomization_FixtureGroupItem.h"
#include "Customizations/DMXPixelMappingDetailCustomization_Screen.h"
#include "Customizations/DMXPixelMappingDetailCustomization_Renderer.h"
#include "Customizations/DMXPixelMappingDetailCustomization_Matrix.h"
#include "Customizations/DMXPixelMappingDetailCustomization_MatrixCell.h"

#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"

void SDMXPixelMappingDetailsView::Construct(const FArguments& InArgs, const TSharedPtr<FDMXPixelMappingToolkit>& InToolkit)
{
	ToolkitWeakPtr = InToolkit;

	// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;

	PropertyView = EditModule.CreateDetailView(DetailsViewArgs);

	RegisterCustomizations();

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			PropertyView.ToSharedRef()
		]
	];

	InToolkit->GetOnSelectedComponentsChangedDelegate().AddRaw(this, &SDMXPixelMappingDetailsView::OnSelectedComponentsChanged);

	OnSelectedComponentsChanged();
}

SDMXPixelMappingDetailsView::~SDMXPixelMappingDetailsView()
{
	if (PropertyView.IsValid())
	{
		PropertyView->UnregisterInstancedCustomPropertyLayout(UDMXPixelMappingFixtureGroupComponent::StaticClass());
		PropertyView->UnregisterInstancedCustomPropertyLayout(UDMXPixelMappingFixtureGroupItemComponent::StaticClass());
		PropertyView->UnregisterInstancedCustomPropertyLayout(UDMXPixelMappingScreenComponent::StaticClass());
		PropertyView->UnregisterInstancedCustomPropertyLayout(UDMXPixelMappingRendererComponent::StaticClass());
	}
}

void SDMXPixelMappingDetailsView::OnSelectedComponentsChanged()
{
	// Clear selection in the property view.
	SelectedObjects.Empty();
	PropertyView->SetObjects(SelectedObjects);

	TSharedPtr<FDMXPixelMappingToolkit> ToolkitPtr = ToolkitWeakPtr.Pin();
	check(ToolkitPtr.IsValid());

	// Add any selected widgets to the list of pending selected objects.
	TSet<FDMXPixelMappingComponentReference> SelectedComponents = ToolkitPtr->GetSelectedComponents();
	if (SelectedComponents.Num() > 0)
	{
		for (const FDMXPixelMappingComponentReference& ComponentRef : SelectedComponents)
		{
			SelectedObjects.Add(ComponentRef.GetComponent());
		}
	}

	// Update the preview view to look at the current selection set.
	const bool bForceRefresh = false;

	bool bHideFilterArea = false;
	for (const TWeakObjectPtr<UObject> Object : SelectedObjects)
	{
		if (UObject* Obj = Object.Get())
		{
			if (Cast<UDMXPixelMappingRootComponent>(Obj))
			{
				bHideFilterArea = true;
				break;
			}
		}
	}

	PropertyView->HideFilterArea(bHideFilterArea);
	PropertyView->SetObjects(SelectedObjects, bForceRefresh);
}

void SDMXPixelMappingDetailsView::RegisterCustomizations()
{
	FOnGetDetailCustomizationInstance FixtureGroupCustomizationInstance = FOnGetDetailCustomizationInstance::CreateStatic(&FDMXPixelMappingDetailCustomization_FixtureGroup::MakeInstance, ToolkitWeakPtr);
	PropertyView->RegisterInstancedCustomPropertyLayout(UDMXPixelMappingFixtureGroupComponent::StaticClass(), FixtureGroupCustomizationInstance);

	FOnGetDetailCustomizationInstance FixtureGroupItemCustomizationInstance = FOnGetDetailCustomizationInstance::CreateStatic(&FDMXPixelMappingDetailCustomization_FixtureGroupItem::MakeInstance, ToolkitWeakPtr);
	PropertyView->RegisterInstancedCustomPropertyLayout(UDMXPixelMappingFixtureGroupItemComponent::StaticClass(), FixtureGroupItemCustomizationInstance);

	FOnGetDetailCustomizationInstance ScreenCustomizationInstance = FOnGetDetailCustomizationInstance::CreateStatic(&FDMXPixelMappingDetailCustomization_Screen::MakeInstance, ToolkitWeakPtr);
	PropertyView->RegisterInstancedCustomPropertyLayout(UDMXPixelMappingScreenComponent::StaticClass(), ScreenCustomizationInstance);

	FOnGetDetailCustomizationInstance RendererCustomizationInstance = FOnGetDetailCustomizationInstance::CreateStatic(&FDMXPixelMappingDetailCustomization_Renderer::MakeInstance, ToolkitWeakPtr);
	PropertyView->RegisterInstancedCustomPropertyLayout(UDMXPixelMappingRendererComponent::StaticClass(), RendererCustomizationInstance);

	FOnGetDetailCustomizationInstance MatrixCustomizationInstance = FOnGetDetailCustomizationInstance::CreateStatic(&FDMXPixelMappingDetailCustomization_Matrix::MakeInstance, ToolkitWeakPtr);
	PropertyView->RegisterInstancedCustomPropertyLayout(UDMXPixelMappingMatrixComponent::StaticClass(), MatrixCustomizationInstance);

	FOnGetDetailCustomizationInstance MatrixCellCustomizationInstance = FOnGetDetailCustomizationInstance::CreateStatic(&FDMXPixelMappingDetailCustomization_MatrixCell::MakeInstance, ToolkitWeakPtr);
	PropertyView->RegisterInstancedCustomPropertyLayout(UDMXPixelMappingMatrixCellComponent::StaticClass(), MatrixCellCustomizationInstance);
}
