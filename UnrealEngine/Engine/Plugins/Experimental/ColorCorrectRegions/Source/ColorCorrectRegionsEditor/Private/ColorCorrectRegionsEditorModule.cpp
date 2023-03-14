// Copyright Epic Games, Inc. All Rights Reserved.

#include "ColorCorrectRegionsEditorModule.h"
#include "Editor/PlacementMode/Public/IPlacementModeModule.h"
#include "ColorCorrectRegion.h"
#include "ColorCorrectWindow.h"
#include "ColorCorrectRegionsStyle.h"
#include "ColorCorrectRegionCustomization.h"
#include "ActorFactories/ActorFactoryBlueprint.h"

#define LOCTEXT_NAMESPACE "FColorCorrectRegionsModule"

void FColorCorrectRegionsEditorModule::StartupModule()
{
	FColorCorrectRegionsStyle::Initialize();
	IPlacementModeModule::Get().OnPlacementModeCategoryRefreshed().AddRaw(this, &FColorCorrectRegionsEditorModule::OnPlacementModeRefresh);

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(AColorCorrectRegion::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FColorCorrectWindowDetails::MakeInstance));
}

void FColorCorrectRegionsEditorModule::OnPlacementModeRefresh(FName CategoryName)
{
	static FName VolumeName = FName(TEXT("Volumes"));
	static FName AllClasses = FName(TEXT("AllClasses"));

	if (CategoryName == VolumeName || CategoryName == AllClasses)
	{
		IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();
		
		FPlaceableItem* CCRPlaceableItem = new FPlaceableItem(
			*AColorCorrectionRegion::StaticClass(),
			FAssetData(AColorCorrectionRegion::StaticClass(), true),
			FName("CCR.PlaceActorThumbnail"),
			FName("CCR.OutlinerThumbnail"),
			TOptional<FLinearColor>(),
			TOptional<int32>(),
			NSLOCTEXT("PlacementMode", "Color Correction Region", "Color Correction Region")
		);

		FPlaceableItem* CCWPlaceableItem = new FPlaceableItem(
			*AColorCorrectionWindow::StaticClass(),
			FAssetData(AColorCorrectionWindow::StaticClass()),
			FName("CCW.PlaceActorThumbnail"),
			FName("CCW.OutlinerThumbnail"),
			TOptional<FLinearColor>(),
			TOptional<int32>(), 
			NSLOCTEXT("PlacementMode", "Color Correction Window", "Color Correction Window"));

		PlacementModeModule.RegisterPlaceableItem(CategoryName, MakeShareable(CCWPlaceableItem));
		PlacementModeModule.RegisterPlaceableItem(CategoryName, MakeShareable(CCRPlaceableItem));
	}
}

void FColorCorrectRegionsEditorModule::ShutdownModule()
{
	FColorCorrectRegionsStyle::Shutdown();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FColorCorrectRegionsEditorModule, ColorCorrectRegionsEditor);
DEFINE_LOG_CATEGORY(ColorCorrectRegionsEditorLogOutput);
