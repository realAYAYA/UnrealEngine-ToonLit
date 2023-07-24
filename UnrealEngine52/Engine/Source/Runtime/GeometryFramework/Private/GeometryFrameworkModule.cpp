// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryFrameworkModule.h"
#include "Components/BaseDynamicMeshComponent.h"
#include "Misc/CoreDelegates.h"

#if WITH_EDITOR

#include "DynamicMeshActor.h"
#include "LevelEditor.h"
#include "Filters/CustomClassFilterData.h"

#endif

#define LOCTEXT_NAMESPACE "FGeometryFrameworkModule"

void FGeometryFrameworkModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FGeometryFrameworkModule::OnPostEngineInit);

#if WITH_EDITOR
	if (!IsRunningGame())
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

		// Register the Level Editor Outliner filter for ADynamicMeshActor
		if(TSharedPtr<FFilterCategory> GeometryFilterCategory = LevelEditorModule.GetOutlinerFilterCategory(FLevelEditorOutlinerBuiltInCategories::Geometry()))
		{
			TSharedRef<FCustomClassFilterData> DynamicMeshActorClassData = MakeShared<FCustomClassFilterData>(ADynamicMeshActor::StaticClass(), GeometryFilterCategory, FLinearColor::White);
			LevelEditorModule.AddCustomClassFilterToOutliner(DynamicMeshActorClassData);
		}
	}
#endif
}

void FGeometryFrameworkModule::OnPostEngineInit()
{
	// UBaseDynamicMeshComponent provides some global materials to all instances, rather than
	// directly accessing (eg) GEngine pointers. Initialize those here. 
	UBaseDynamicMeshComponent::InitializeDefaultMaterials();
}

void FGeometryFrameworkModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGeometryFrameworkModule, GeometryFramework)