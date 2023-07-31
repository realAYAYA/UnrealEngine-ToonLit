// Copyright Epic Games, Inc. All Rights Reserved.

#include "LightGizmosModule.h"
#include "ContextObjectStore.h"
#include "Editor.h"
#include "EditorModeRegistry.h"
#include "EditorModeManager.h"
#include "Misc/CoreDelegates.h"
#include "GizmoEdMode.h"
#include "BaseGizmos/GizmoViewContext.h"
#include "PointLightGizmoFactory.h"
#include "DirectionalLightGizmoFactory.h"
#include "SpotLightGizmoFactory.h"
#include "PointLightGizmo.h"
#include "ScalableConeGizmo.h"
#include "SpotLightGizmo.h"
#include "DirectionalLightGizmo.h"
#include "LevelEditor.h"
#include "EdModeInteractiveToolsContext.h"
#include "InteractiveGizmoManager.h"

#define LOCTEXT_NAMESPACE "FLightGizmosModule"

FString FLightGizmosModule::PointLightGizmoType = TEXT("PointLightGizmoType");
FString FLightGizmosModule::SpotLightGizmoType = TEXT("SpotLightGizmoType");
FString FLightGizmosModule::ScalableConeGizmoType = TEXT("ScalableConeGizmoType");
FString FLightGizmosModule::DirectionalLightGizmoType = TEXT("DirectionalLightGizmoType");

void FLightGizmosModule::OnLevelEditorCreated(TSharedPtr<ILevelEditor> InLevelEditor)
{
	TWeakPtr<ILevelEditor> LevelEditorPtr = InLevelEditor;
	InLevelEditor->GetEditorModeManager().OnEditorModeIDChanged().AddLambda([LevelEditorPtr](const FEditorModeID& ModeID, bool IsEnteringMode)
	{
		if (ModeID == GetDefault<UGizmoEdMode>()->GetID() && LevelEditorPtr.IsValid())
		{
			UEdMode* EdMode = LevelEditorPtr.Pin()->GetEditorModeManager().GetActiveScriptableMode(GetDefault<UGizmoEdMode>()->GetID());
			UGizmoEdMode* GizmoEdMode = Cast<UGizmoEdMode>(EdMode);

			 // Register the factories and gizmos if we are entering the new Gizmo Mode
			if (IsEnteringMode && GizmoEdMode)
			{
				// UGizmoViewContext is needed for DirectionalLightGizmo and SpotLightGizmo
				UContextObjectStore* ContextStore = LevelEditorPtr.Pin()->GetEditorModeManager().GetInteractiveToolsContext()->ContextObjectStore;
				UGizmoViewContext* GizmoViewContext = ContextStore->FindContext<UGizmoViewContext>();
				if (!GizmoViewContext)
				{
					GizmoViewContext = NewObject<UGizmoViewContext>();
					ContextStore->AddContextObject(GizmoViewContext);
				}

				LevelEditorPtr.Pin()->GetEditorModeManager().GetInteractiveToolsContext()->GizmoManager->RegisterGizmoType(PointLightGizmoType, NewObject<UPointLightGizmoBuilder>());
				LevelEditorPtr.Pin()->GetEditorModeManager().GetInteractiveToolsContext()->GizmoManager->RegisterGizmoType(ScalableConeGizmoType, NewObject<UScalableConeGizmoBuilder>());
				LevelEditorPtr.Pin()->GetEditorModeManager().GetInteractiveToolsContext()->GizmoManager->RegisterGizmoType(SpotLightGizmoType, NewObject<USpotLightGizmoBuilder>());
				LevelEditorPtr.Pin()->GetEditorModeManager().GetInteractiveToolsContext()->GizmoManager->RegisterGizmoType(DirectionalLightGizmoType, NewObject<UDirectionalLightGizmoBuilder>());
				GizmoEdMode->AddFactory(NewObject<UPointLightGizmoFactory>());
				GizmoEdMode->AddFactory(NewObject<UDirectionalLightGizmoFactory>());
				GizmoEdMode->AddFactory(NewObject<USpotLightGizmoFactory>());
			}
			else
			{
				LevelEditorPtr.Pin()->GetEditorModeManager().GetInteractiveToolsContext()->GizmoManager->DeregisterGizmoType(PointLightGizmoType);
				LevelEditorPtr.Pin()->GetEditorModeManager().GetInteractiveToolsContext()->GizmoManager->DeregisterGizmoType(ScalableConeGizmoType);
				LevelEditorPtr.Pin()->GetEditorModeManager().GetInteractiveToolsContext()->GizmoManager->DeregisterGizmoType(SpotLightGizmoType);
				LevelEditorPtr.Pin()->GetEditorModeManager().GetInteractiveToolsContext()->GizmoManager->DeregisterGizmoType(DirectionalLightGizmoType);
			}
			
		}
	});

}

void FLightGizmosModule::StartupModule()
{
	FLevelEditorModule& LevelEditor = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditor.OnLevelEditorCreated().AddRaw(this, &FLightGizmosModule::OnLevelEditorCreated);
}

void FLightGizmosModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLightGizmosModule, LightGizmos);
