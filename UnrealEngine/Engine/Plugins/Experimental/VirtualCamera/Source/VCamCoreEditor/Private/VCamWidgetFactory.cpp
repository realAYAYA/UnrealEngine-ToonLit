// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamWidgetFactory.h"
#include "UI/VCamWidget.h"

#include "AssetToolsModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "VCamWidgetFactory"

UVCamWidgetFactory::UVCamWidgetFactory()
{
	SupportedClass = UBlueprint::StaticClass();
	ParentClass = UVCamWidget::StaticClass();

	bCreateNew = true;
	bEditAfterNew = true;
}

FText UVCamWidgetFactory::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "VCam Widget");
}

FText UVCamWidgetFactory::GetToolTip() const
{
	return LOCTEXT("Tooltip", "A wrapper widget class that contains a set of VCam Connections\n\nIf you add a widget deriving from VCam Widget to an Overlay Widget for a VCam Output Provider then when the Overlay is created by the Provider it will also call InitializeConnections with the owning VCam Component.");
}

UObject* UVCamWidgetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UBlueprint* WidgetBlueprint = nullptr;
	if (ensure(SupportedClass == Class))
	{
		ensure(0 != (RF_Public & Flags));
		WidgetBlueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass(), NAME_None);
		if (TSubclassOf<UObject> GeneratedClass = WidgetBlueprint->GeneratedClass)
		{
			if (UVCamWidget* DefaultSubject = GeneratedClass->GetDefaultObject<UVCamWidget>())
			{
				DefaultSubject->InputMappingContext = InputMappingContext;
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
	}
	return WidgetBlueprint;	
}

uint32 UVCamWidgetFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return AssetTools.RegisterAdvancedAssetCategory("VirtualCamera", LOCTEXT("AssetCategoryName", "VCam"));
}

#undef LOCTEXT_NAMESPACE
