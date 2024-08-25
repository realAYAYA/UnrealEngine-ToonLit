// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDModule.h"

#include "ChaosVDCommands.h"
#include "ChaosVDStyle.h"
#include "ChaosVDParticleActorCustomization.h"
#include "ChaosVDTabsIDs.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SChaosVDMainTab.h"
#include "PropertyEditorModule.h"
#include "ChaosVDEngine.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "DetailsCustomizations/ChaosVDGeometryComponentCustomization.h"
#include "DetailsCustomizations/ChaosVDParticleDataWrapperCustomization.h"
#include "DetailsCustomizations/ChaosVDQueryDataWrappersCustomizationDetails.h"
#include "Trace/ChaosVDTraceManager.h"
#include "Misc/Guid.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

DEFINE_LOG_CATEGORY(LogChaosVDEditor);

FAutoConsoleCommand ChaosVDSpawnNewCVDInstance(
	TEXT("p.Chaos.VD.SpawnNewCVDInstance"),
	TEXT("Opens a new CVD windows wothout closing an existing one"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		FChaosVDModule::Get().SpawnCVDTab();
	})
);

FChaosVDModule& FChaosVDModule::Get()
{
	return FModuleManager::Get().LoadModuleChecked<FChaosVDModule>(TEXT("ChaosVD"));
}

void FChaosVDModule::StartupModule()
{	
	FChaosVDStyle::Initialize();
	
	FChaosVDCommands::Register();

	RegisterClassesCustomDetails();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(FChaosVDTabID::ChaosVisualDebuggerTab, FOnSpawnTab::CreateRaw(this, &FChaosVDModule::SpawnMainTab))
								.SetDisplayName(LOCTEXT("VisualDebuggerTabTitle", "Chaos Visual Debugger"))
								.SetTooltipText(LOCTEXT("VisualDebuggerTabDesc", "Opens the Chaos Visual Debugger window"))
								.SetIcon(FSlateIcon(FChaosVDStyle::GetStyleSetName(), "ChaosVisualDebugger"))
								.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());

	ChaosVDTraceManager = MakeShared<FChaosVDTraceManager>();

	FCoreDelegates::OnEnginePreExit.AddRaw(this, &FChaosVDModule::CloseActiveInstances);
}

void FChaosVDModule::ShutdownModule()
{
	FChaosVDStyle::Shutdown();
	
	FChaosVDCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(FChaosVDTabID::ChaosVisualDebuggerTab);

	for (const FName TabID : CreatedExtraTabSpawnersIDs)
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabID);
	}
	
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);

	CloseActiveInstances();
}

void FChaosVDModule::RegisterClassesCustomDetails() const
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("ChaosVDParticleActor", FOnGetDetailCustomizationInstance::CreateStatic(&FChaosVDParticleActorCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("ChaosVDInstancedStaticMeshComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FChaosVDGeometryComponentCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("ChaosVDStaticMeshComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FChaosVDGeometryComponentCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("ChaosVDQueryVisitStep", FOnGetDetailCustomizationInstance::CreateStatic(&FChaosVDQueryVisitDataCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("ChaosVDQueryDataWrapper", FOnGetDetailCustomizationInstance::CreateStatic(&FChaosVDQueryDataWrapperCustomization::MakeInstance));

	//TODO: Rename FChaosVDParticleDataWrapperCustomization to something generic as currently works with any type that wants to hide properties of type FChaosVDWrapperDataBase with invalid data.
	// Or another option is create a new custom layout intended to be generic from the get go
	PropertyModule.RegisterCustomPropertyTypeLayout("ChaosVDQueryDataWrapper", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FChaosVDParticleDataWrapperCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("ChaosVDQueryVisitStep", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FChaosVDParticleDataWrapperCustomization::MakeInstance));
}

void FChaosVDModule::SpawnCVDTab()
{
	// Registering new tab spawners with random names is not the best idea to spawn new tabs, but it is good enough to test we can run multiple instances of CVD withing the editor.
	// This is also why spawning new tabs it is only exposed via console commands for now.
	// When this feature is deemed stable and exposed in the UI I will investigate a more correct way of implementing this
	const FName NewTabID = FName(FChaosVDTabID::ChaosVisualDebuggerTab.ToString() + FGuid::NewGuid().ToString());

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(NewTabID, FOnSpawnTab::CreateRaw(this, &FChaosVDModule::SpawnMainTab))
						.SetDisplayName(LOCTEXT("VisualDebuggerTabTitle", "Chaos Visual Debugger"))
						.SetTooltipText(LOCTEXT("VisualDebuggerTabDesc", "Opens the Chaos Visual Debugger window"));

	FGlobalTabmanager::Get()->TryInvokeTab(NewTabID);

	CreatedExtraTabSpawnersIDs.Add(NewTabID);
}

TSharedRef<SDockTab> FChaosVDModule::SpawnMainTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> MainTabInstance =
		SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(LOCTEXT("MainTabLabel", "Chaos Visual Debugger"))
		.ToolTipText(LOCTEXT("MainTabToolTip", "Chaos Visual Debugger is an experimental tool and it can be unstable"));

	// Initialize the Chaos VD Engine instance this tab will represent
	// For now its lifetime will be controlled by this tab
	const TSharedPtr<FChaosVDEngine> ChaosVDEngineInstance = MakeShared<FChaosVDEngine>();
	ChaosVDEngineInstance->Initialize();

	MainTabInstance->SetContent
	(
		SNew(SChaosVDMainTab, ChaosVDEngineInstance)
			.OwnerTab(MainTabInstance.ToSharedPtr())
	);

	const FGuid InstanceGuid = ChaosVDEngineInstance->GetInstanceGuid();
	RegisterChaosVDEngineInstance(InstanceGuid, ChaosVDEngineInstance);

	const SDockTab::FOnTabClosedCallback ClosedCallback = SDockTab::FOnTabClosedCallback::CreateRaw(this, &FChaosVDModule::HandleTabClosed, InstanceGuid);
	MainTabInstance->SetOnTabClosed(ClosedCallback);

	RegisterChaosVDTabInstance(InstanceGuid, MainTabInstance.ToSharedPtr());

	return MainTabInstance;
}

void FChaosVDModule::HandleTabClosed(TSharedRef<SDockTab> ClosedTab, FGuid InstanceGUID)
{
	// Workaround. Currently the ChaosVD Engine instance determines the lifetime of the Editor world and other objects
	// Some widgets, like UE Level viewport tries to iterate on these objects on destruction
	// For now we can avoid any crashes by just de-initializing ChaosVD Engine on the next frame but that is not the real fix.
	// Unless we are shutting down the engine
	
	//TODO: Ensure that systems that uses the Editor World we create know beforehand when it is about to be Destroyed and GC'd
	// Related Jira Task UE-191876
	if (bIsShuttingDown)
	{
		DeregisterChaosVDTabInstance(InstanceGUID);
		DeregisterChaosVDEngineInstance(InstanceGUID);
	}
	else
	{
		DeregisterChaosVDTabInstance(InstanceGUID);

		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this, InstanceGUID](float DeltaTime)->bool
		{
			DeregisterChaosVDEngineInstance(InstanceGUID);
			return false;
		}));
	}
}

void FChaosVDModule::RegisterChaosVDEngineInstance(const FGuid& InstanceGuid, TSharedPtr<FChaosVDEngine> Instance)
{
	ActiveChaosVDInstances.Add(InstanceGuid, Instance);
}

void FChaosVDModule::DeregisterChaosVDEngineInstance(const FGuid& InstanceGuid)
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

void FChaosVDModule::RegisterChaosVDTabInstance(const FGuid& InstanceGuid, TSharedPtr<SDockTab> Instance)
{
	ActiveCVDTabs.Add(InstanceGuid, Instance);
}

void FChaosVDModule::DeregisterChaosVDTabInstance(const FGuid& InstanceGuid)
{
	ActiveCVDTabs.Remove(InstanceGuid);
}

void FChaosVDModule::CloseActiveInstances()
{
	bIsShuttingDown = true;
	for (const TPair<FGuid, TWeakPtr<SDockTab>>& CVDTabWithID : ActiveCVDTabs)
	{
		if (TSharedPtr<SDockTab> CVDTab = CVDTabWithID.Value.Pin())
		{
			CVDTab->RequestCloseTab();
		}
		else
		{
			// if the tab Instance no longer exist, make sure the CVD engine instance is shutdown
			DeregisterChaosVDEngineInstance(CVDTabWithID.Key);
		}
	}

	ActiveChaosVDInstances.Reset();
	ActiveCVDTabs.Reset();
}


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FChaosVDModule, ChaosVD)