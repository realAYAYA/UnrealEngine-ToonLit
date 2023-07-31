// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/DMXPixelMappingPaletteViewModel.h"

#include "Components/DMXPixelMappingOutputComponent.h"
#include "Templates/DMXPixelMappingComponentTemplate.h"
#include "Widgets/SDMXPixelMappingPaletteItem.h"
#include "DMXPixelMappingComponentReference.h"

#include "UObject/UObjectIterator.h"

void FDMXPixelMappingPaletteViewModel::Update()
{
	if (bRebuildRequested)
	{
		BuildWidgetList();

		bRebuildRequested = false;
	}
}

void FDMXPixelMappingPaletteViewModel::BuildWidgetList()
{
	WidgetViewModels.Reset();
	WidgetTemplateCategories.Reset();

	BuildClassWidgetList();

	for (TPair<FString, FDMXPixelMappingComponentTemplateArray>& Category : WidgetTemplateCategories)
	{
		TSharedPtr<FDMXPixelMappingPaletteWidgetViewModelHeader> Header = MakeShared<FDMXPixelMappingPaletteWidgetViewModelHeader>();
		Header->GroupName = FText::FromString(Category.Key);

		for (FDMXPixelMappingComponentTemplatePtr& ComponentTemplate : Category.Value)
		{
			TSharedPtr<FDMXPixelMappingPaletteWidgetViewModelTemplate> WidgetViewModelTemplate = MakeShared<FDMXPixelMappingPaletteWidgetViewModelTemplate>();
			WidgetViewModelTemplate->Template = ComponentTemplate;
			Header->Children.Add(WidgetViewModelTemplate);
		}

		WidgetViewModels.Add(Header);
	}
}

void FDMXPixelMappingPaletteViewModel::BuildClassWidgetList()
{
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* WidgetClass = *ClassIt;

		if (WidgetClass->IsChildOf(UDMXPixelMappingOutputComponent::StaticClass()))
		{
			UDMXPixelMappingOutputComponent* OutputComponent = WidgetClass->GetDefaultObject<UDMXPixelMappingOutputComponent>();

			if (OutputComponent->IsExposedToTemplate())
			{
				TSharedPtr<FDMXPixelMappingComponentTemplate> Template = MakeShared<FDMXPixelMappingComponentTemplate>(WidgetClass);
				AddWidgetTemplate(Template);
			}
		}
	}
}

void FDMXPixelMappingPaletteViewModel::AddWidgetTemplate(FDMXPixelMappingComponentTemplatePtr Template)
{
	FString Category = Template->GetCategory().ToString();

	FDMXPixelMappingComponentTemplateArray& Group = WidgetTemplateCategories.FindOrAdd(Category);
	Group.Add(Template);
}

TSharedRef<ITableRow> FDMXPixelMappingPaletteWidgetViewModelHeader::BuildRow(const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SDMXPixelMappingHierarchyItemHeader, OwnerTable, SharedThis(this));
}

void FDMXPixelMappingPaletteWidgetViewModelHeader::GetChildren(FDMXPixelMappingPreviewWidgetViewModelArray& OutChildren)
{
	for (FDMXPixelMappingPreviewWidgetViewModelPtr& Child : Children)
	{
		OutChildren.Add(Child);
	}
}

FText FDMXPixelMappingPaletteWidgetViewModelTemplate::GetName() const
{ 
	return Template.Pin()->Name; 
}

TSharedRef<ITableRow> FDMXPixelMappingPaletteWidgetViewModelTemplate::BuildRow(const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SDMXPixelMappingHierarchyItemTemplate, OwnerTable, SharedThis(this));
}
