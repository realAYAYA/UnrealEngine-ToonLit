// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceDataObjectFixupToolModule.h"
#include "InstanceDataObjectFixupTool.h"

#define LOCTEXT_NAMESPACE "InstanceDataObjectFixupToolModule"

static const FName InstanceDataObjectFixupToolTabName = FName(TEXT("InstanceDataObjectFixupTool"));

void FInstanceDataObjectFixupToolModule::StartupModule()
{
}

void FInstanceDataObjectFixupToolModule::ShutdownModule()
{
	
}

bool FInstanceDataObjectFixupToolModule::OpenInstanceDataObjectFixupTool() const
{
	TSharedPtr<SDockTab> DockTab = FGlobalTabmanager::Get()->TryInvokeTab(FTabId(InstanceDataObjectFixupToolTabName));
	if (!DockTab)
	{
		return false;
	}
	DockTab->DrawAttention();
	return true;
}

TSharedRef<SDockTab> FInstanceDataObjectFixupToolModule::CreateInstanceDataObjectFixupTab(const FSpawnTabArgs& TabArgs, TConstArrayView<TObjectPtr<UObject>> InstanceDataObjects) const
{
	const TSharedRef<SInstanceDataObjectFixupTool> InstanceDataObjectFixupTool = SNew(SInstanceDataObjectFixupTool)
		.InstanceDataObjects(InstanceDataObjects);
	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			InstanceDataObjectFixupTool
		];

	InstanceDataObjectFixupTool->SetDockTab(DockTab);
	InstanceDataObjectFixupTool->GenerateDetailsViews();
	
	return DockTab;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FInstanceDataObjectFixupToolModule, InstanceDataObjectFixupTool)
