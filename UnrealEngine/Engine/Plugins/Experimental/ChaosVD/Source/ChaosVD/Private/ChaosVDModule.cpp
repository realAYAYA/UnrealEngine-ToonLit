// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDModule.h"
#include "ChaosVDStyle.h"
#include "ChaosVDParticleActorCustomization.h"
#include "ChaosVDTabsIDs.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SChaosVDMainTab.h"
#include "PropertyEditorModule.h"
#include "ChaosVDEngine.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "DetailsCustomizations/ChaosVDParticleDataWrapperCustomization.h"
#include "Trace/ChaosVDTraceManager.h"
#include "Misc/Guid.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

DEFINE_LOG_CATEGORY(LogChaosVDEditor);

FChaosVDModule& FChaosVDModule::Get()
{
	return FModuleManager::Get().LoadModuleChecked<FChaosVDModule>(TEXT("ChaosVD"));
}

void FChaosVDModule::StartupModule()
{	
	FChaosVDStyle::Initialize();
	FChaosVDStyle::ReloadTextures();

	RegisterClassesCustomDetails();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(FChaosVDTabID::ChaosVisualDebuggerTab, FOnSpawnTab::CreateRaw(this, &FChaosVDModule::SpawnMainTab))
								.SetDisplayName(LOCTEXT("VisualDebuggerTabTitle", "Chaos Visual Debugger"))
								.SetTooltipText(LOCTEXT("VisualDebuggerTabDesc", "Opens the Chaos Visual Debugger window"))
								//TODO: Hook up the final icon
								.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "CollisionAnalyzer.TabIcon"))
								.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());

	ChaosVDTraceManager = MakeShared<FChaosVDTraceManager>();
}

void FChaosVDModule::ShutdownModule()
{
	FChaosVDStyle::Shutdown();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(FChaosVDTabID::ChaosVisualDebuggerTab);
}

void FChaosVDModule::RegisterClassesCustomDetails() const
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("ChaosVDParticleActor", FOnGetDetailCustomizationInstance::CreateStatic(&FChaosVDParticleActorCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("ChaosVDParticleDataWrapper", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FChaosVDParticleDataWrapperCustomization::MakeInstance));
}

TSharedRef<SDockTab> FChaosVDModule::SpawnMainTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> MainTabInstance =
		SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(LOCTEXT("MainTabLabel", "Chaos Visual Debugger"))
		.ToolTipText(LOCTEXT("MainTabToolTip", "The Chaos Visual debugger is under development"));

	// Initialize the Chaos VD Engine instance this tab will represent
	// For now its lifetime will be controlled by this tab
	const TSharedPtr<FChaosVDEngine> ChaosVDEngineInstance = MakeShared<FChaosVDEngine>();
	ChaosVDEngineInstance->Initialize();

	MainTabInstance->SetContent
	(
		SNew(SChaosVDMainTab, ChaosVDEngineInstance)
			.OwnerTab(MainTabInstance.ToSharedPtr())
	);

	MainTabInstance->SetTabIcon(FChaosVDStyle::Get().GetBrush("TabIconPlaybackViewport"));

	const FGuid InstanceGuid = ChaosVDEngineInstance->GetInstanceGuid();
	RegisterChaosVDInstance(InstanceGuid, ChaosVDEngineInstance);

	// Workaround. Currently the ChaosVD Engine instance determines the lifetime of the Editor world and other objects
	// Some widgets, like UE Level viewport tries to iterate on these objects on destruction
	// For now we can avoid any crashes by just de-initializing ChaosVD Engine on the next frame but that is not the real fix.
	
	//TODO: Ensure that systems that uses the Editor World we create know beforehand when it is about to be Destroyed and GC'd
	MainTabInstance->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda( [this, InstanceGuid](TSharedRef<SDockTab>)
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this, InstanceGuid](float DeltaTime)->bool
		{
			DeregisterChaosVDInstance(InstanceGuid);
			return false;
		}));
	}));

	return MainTabInstance;
}

void FChaosVDModule::RegisterChaosVDInstance(const FGuid& InstanceGuid, TSharedPtr<FChaosVDEngine> Instance)
{
	ActiveChaosVDInstances.Add(InstanceGuid, Instance);
}

void FChaosVDModule::DeregisterChaosVDInstance(const FGuid& InstanceGuid)
{
	if (TSharedPtr<FChaosVDEngine>* InstancePtrPtr = ActiveChaosVDInstances.Find(InstanceGuid))
	{
		if (TSharedPtr<FChaosVDEngine> InstancePtr = *InstancePtrPtr)
		{
			InstancePtr->DeInitialize();
		}
	
		ActiveChaosVDInstances.Remove(InstanceGuid);
	}	
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FChaosVDModule, ChaosVD)