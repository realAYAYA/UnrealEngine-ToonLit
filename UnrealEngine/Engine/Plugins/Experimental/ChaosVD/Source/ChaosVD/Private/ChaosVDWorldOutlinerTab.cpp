// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDWorldOutlinerTab.h"

#include "ChaosVDWorldOutlinerMode.h"
#include "ChaosVDStyle.h"
#include "Modules/ModuleManager.h"
#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"
#include "Widgets/Docking/SDockTab.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

void FChaosVDWorldOutlinerTab::CreateWorldOutlinerWidget()
{
	FSceneOutlinerInitializationOptions InitOptions;
	InitOptions.bShowTransient = true;

	InitOptions.OutlinerIdentifier = TEXT("ChaosVDOutliner");
	InitOptions.FilterBarOptions.bHasFilterBar = true;
	InitOptions.FilterBarOptions.bUseSharedSettings = false;

	InitOptions.UseDefaultColumns();
	InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Gutter(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::Gutter_Localized()));
	InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::ActorInfo(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 20, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::ActorInfo_Localized()));
		
	FCreateSceneOutlinerMode ModeFactory = FCreateSceneOutlinerMode::CreateLambda([this](SSceneOutliner* Outliner)
	{
		FActorModeParams ModeParams(Outliner);
		ModeParams.SpecifiedWorldToDisplay = GetChaosVDWorld();
		ModeParams.bHideEmptyFolders = true;

		// The mode is deleted by the Outliner when it is destroyed 
		return new FChaosVDWorldOutlinerMode(ModeParams, GetChaosVDScene());
	});
	InitOptions.ModeFactory = ModeFactory;
	
	const FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::Get().LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");

	SceneOutlinerWidget = SceneOutlinerModule.CreateSceneOutliner(InitOptions);
}

TSharedRef<SDockTab> FChaosVDWorldOutlinerTab::HandleTabSpawned(const FSpawnTabArgs& Args)
{
	CreateWorldOutlinerWidget();

	TSharedRef<SDockTab> OutlinerTab =
		SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(LOCTEXT("Physics World Outliner", "Physics World Outliner"))
		.ToolTipText(LOCTEXT("PhysicsWorldOutlinerTabToolTip", "Hierachy view of the physics objects by category"));
	
	OutlinerTab->SetContent
	(
		SceneOutlinerWidget.ToSharedRef()
	);

	OutlinerTab->SetTabIcon(FChaosVDStyle::Get().GetBrush("TabIconWorldOutliner"));

	return OutlinerTab;
}

#undef LOCTEXT_NAMESPACE
