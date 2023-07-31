// Copyright Epic Games, Inc. All Rights Reserved.

#include "VPMaterialsEditorModule.h"
#include "SMaterialDynamicWidgets.h"

#include "MaterialList.h"

LLM_DEFINE_TAG(VirtualProduction_VPMaterialsEditor);

void FVPMaterialsEditorModule::StartupModule()
{
	LLM_SCOPE_BYTAG(VirtualProduction_VPMaterialsEditor);

	// Add bottom extender for material item
	FMaterialList::OnAddMaterialItemViewExtraBottomWidget.AddLambda([](const TSharedRef<FMaterialItemView>& InMaterialItemView, UActorComponent* InCurrentComponent, IDetailLayoutBuilder& InDetailBuilder, TArray<TSharedPtr<SWidget>>& OutExtensions)
	{
		LLM_SCOPE_BYTAG(VirtualProduction_VPMaterialsEditor);
		OutExtensions.Add(SNew(SMaterialDynamicView, InMaterialItemView, InCurrentComponent));
	});
}

void FVPMaterialsEditorModule::ShutdownModule()
{
	LLM_SCOPE_BYTAG(VirtualProduction_VPMaterialsEditor);
	FMaterialList::OnAddMaterialItemViewExtraBottomWidget.RemoveAll(this);
}

IMPLEMENT_MODULE(FVPMaterialsEditorModule, VPMaterialsEditor)

			