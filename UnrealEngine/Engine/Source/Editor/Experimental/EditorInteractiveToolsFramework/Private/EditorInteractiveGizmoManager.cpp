// Copyright Epic Games, Inc. All Rights Reserved.


#include "EditorInteractiveGizmoManager.h"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorInteractiveGizmoConditionalBuilder.h"
#include "EditorInteractiveGizmoSelectionBuilder.h"
#include "EditorInteractiveGizmoSubsystem.h"
#include "EditorModeManager.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "HAL/IConsoleManager.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "InputRouter.h"
#include "InteractiveGizmo.h"
#include "InteractiveGizmoBuilder.h"
#include "Misc/AssertionMacros.h"
#include "SceneView.h"
#include "ShowFlags.h"
#include "Templates/Casts.h"
#include "ToolContextInterfaces.h"

class FCanvas;

#define LOCTEXT_NAMESPACE "UEditorInteractiveGizmoManager"

static TAutoConsoleVariable<int32> CVarUseLegacyWidget(
	TEXT("Gizmos.UseLegacyWidget"),
	1,
	TEXT("Specify whether to use selection-based gizmos or legacy widget\n")
	TEXT("0 = enable UE5 transform and other selection-based gizmos.\n")
	TEXT("1 = enable legacy UE4 transform widget."),
	ECVF_RenderThreadSafe);


UEditorInteractiveGizmoManager::UEditorInteractiveGizmoManager() :
	UInteractiveGizmoManager()
{
	Registry = NewObject<UEditorInteractiveGizmoRegistry>();
}


void UEditorInteractiveGizmoManager::InitializeWithEditorModeManager(IToolsContextQueriesAPI* QueriesAPIIn, IToolsContextTransactionsAPI* TransactionsAPIIn, UInputRouter* InputRouterIn, FEditorModeTools* InEditorModeManager)
{
	Super::Initialize(QueriesAPIIn, TransactionsAPIIn, InputRouterIn);
	EditorModeManager = InEditorModeManager;
}


void UEditorInteractiveGizmoManager::Shutdown()
{
	Registry->Shutdown();
	Super::Shutdown();
}

void UEditorInteractiveGizmoManager::RegisterEditorGizmoType(EEditorGizmoCategory InGizmoCategory, UInteractiveGizmoBuilder* InGizmoBuilder)
{
	check(Registry);
	Registry->RegisterEditorGizmoType(InGizmoCategory, InGizmoBuilder);
}

void UEditorInteractiveGizmoManager::DeregisterEditorGizmoType(EEditorGizmoCategory InGizmoCategory, UInteractiveGizmoBuilder* InGizmoBuilder)
{
	check(Registry);
	Registry->DeregisterEditorGizmoType(InGizmoCategory, InGizmoBuilder);
}

void UEditorInteractiveGizmoManager::GetQualifiedEditorGizmoBuilders(EEditorGizmoCategory InGizmoCategory, const FToolBuilderState& InToolBuilderState, TArray<UInteractiveGizmoBuilder*>& InFoundBuilders)
{
	check(Registry);
	Registry->GetQualifiedEditorGizmoBuilders(InGizmoCategory, InToolBuilderState, InFoundBuilders);

	FEditorGizmoTypePriority FoundPriority = 0;

	if (!bSearchLocalBuildersOnly)
	{
		UEditorInteractiveGizmoSubsystem* GizmoSubsystem = GEditor->GetEditorSubsystem<UEditorInteractiveGizmoSubsystem>();
		if (ensure(GizmoSubsystem))
		{
			GizmoSubsystem->GetQualifiedGlobalEditorGizmoBuilders(InGizmoCategory, InToolBuilderState, InFoundBuilders);
		}
	}
}

TArray<UInteractiveGizmo*> UEditorInteractiveGizmoManager::CreateGizmosForCurrentSelectionSet()
{
	if (bShowEditorGizmos)
	{
		FToolBuilderState CurrentSceneState;
		QueriesAPI->GetCurrentSelectionState(CurrentSceneState);

		TArray<TObjectPtr<UInteractiveGizmoBuilder>> FoundBuilders;
		GetQualifiedEditorGizmoBuilders(EEditorGizmoCategory::Primary, CurrentSceneState, FoundBuilders);

		bool bHasPrimaryBuilder = (FoundBuilders.Num() > 0);

		if (!bHasPrimaryBuilder)
		{
			GetQualifiedEditorGizmoBuilders(EEditorGizmoCategory::Accessory, CurrentSceneState, FoundBuilders);

			UInteractiveGizmoBuilder* TransformBuilder = GetTransformGizmoBuilder();

			FoundBuilders.Add(TransformBuilder);
		}

		TArray<TObjectPtr<UInteractiveGizmo>> NewGizmos;

		for (UInteractiveGizmoBuilder* FoundBuilder : FoundBuilders)
		{
			if (UInteractiveGizmo* NewGizmo = FoundBuilder->BuildGizmo(CurrentSceneState))
			{
				if (IEditorInteractiveGizmoSelectionBuilder* SelectionBuilder = Cast<IEditorInteractiveGizmoSelectionBuilder>(FoundBuilder))
				{
					SelectionBuilder->UpdateGizmoForSelection(NewGizmo, CurrentSceneState);
				}

				// register new active input behaviors
				InputRouter->RegisterSource(NewGizmo);

				NewGizmos.Add(NewGizmo);
			}
		}

		PostInvalidation();

		for (UInteractiveGizmo* NewGizmo : NewGizmos)
		{
			FActiveEditorGizmo ActiveGizmo = { NewGizmo, nullptr };
			ActiveEditorGizmos.Add(ActiveGizmo);
		}

		return NewGizmos;
	}

	return TArray<UInteractiveGizmo*>();
}

UInteractiveGizmoBuilder* UEditorInteractiveGizmoManager::GetTransformGizmoBuilder()
{
	UEditorInteractiveGizmoSubsystem* GizmoSubsystem = GEditor->GetEditorSubsystem<UEditorInteractiveGizmoSubsystem>();
	if (ensure(GizmoSubsystem))
	{
		return GizmoSubsystem->GetTransformGizmoBuilder();
	}

	return nullptr;
}

bool UEditorInteractiveGizmoManager::DestroyEditorGizmo(UInteractiveGizmo* Gizmo)
{
	auto Pred = [Gizmo](const FActiveEditorGizmo& ActiveEditorGizmo) {return ActiveEditorGizmo.Gizmo == Gizmo; };
	if (!ensure(ActiveEditorGizmos.FindByPredicate(Pred)))
	{
		return false;
	}

	InputRouter->ForceTerminateSource(Gizmo);

	Gizmo->Shutdown();

	InputRouter->DeregisterSource(Gizmo);

	ActiveEditorGizmos.RemoveAll(Pred);

	PostInvalidation();

	return true;
}

void UEditorInteractiveGizmoManager::DestroyAllEditorGizmos()
{
	for (int i = 0; i < ActiveEditorGizmos.Num(); i++)
	{
		UInteractiveGizmo* Gizmo = ActiveEditorGizmos[i].Gizmo;
		if (ensure(Gizmo))
		{
			DestroyEditorGizmo(Gizmo);
		}
	}

	ActiveEditorGizmos.Reset();

	PostInvalidation();
}

void UEditorInteractiveGizmoManager::HandleEditorSelectionSetChanged(const UTypedElementSelectionSet* InSelectionSet)
{
	DestroyAllEditorGizmos();

	if (InSelectionSet && InSelectionSet->HasSelectedElements())
	{
		CreateGizmosForCurrentSelectionSet();
	}
}

// @todo move this to a gizmo context object
bool UEditorInteractiveGizmoManager::GetShowEditorGizmos()
{
	return bShowEditorGizmos;
}

bool UEditorInteractiveGizmoManager::GetShowEditorGizmosForView(IToolsContextRenderAPI* RenderAPI)
{
	const bool bEngineShowFlagsModeWidget = (RenderAPI && RenderAPI->GetSceneView() && 
											 RenderAPI->GetSceneView()->Family &&
											 RenderAPI->GetSceneView()->Family->EngineShowFlags.ModeWidgets);
	return bShowEditorGizmos && bEngineShowFlagsModeWidget;
}

void UEditorInteractiveGizmoManager::UpdateActiveEditorGizmos()
{
	const bool bEditorModeToolsSupportsWidgetDrawing = EditorModeManager ? EditorModeManager->GetShowWidget() : true;
	const bool bEnableEditorGizmos = (CVarUseLegacyWidget.GetValueOnGameThread() == 0);
	const bool bNewShowEditorGizmos = bEditorModeToolsSupportsWidgetDrawing && bEnableEditorGizmos;

	if (bShowEditorGizmos != bNewShowEditorGizmos)
	{
		bShowEditorGizmos = bNewShowEditorGizmos;
		if (bShowEditorGizmos)
		{
			CreateGizmosForCurrentSelectionSet();
		}
		else
		{
			DestroyAllEditorGizmos();
		}
	}
}

void UEditorInteractiveGizmoManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateActiveEditorGizmos();

	for (FActiveEditorGizmo& ActiveEditorGizmo : ActiveEditorGizmos)
	{
		ActiveEditorGizmo.Gizmo->Tick(DeltaTime);
	}
}


void UEditorInteractiveGizmoManager::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);

	if (GetShowEditorGizmosForView(RenderAPI))
	{
		for (FActiveEditorGizmo& ActiveEditorGizmo : ActiveEditorGizmos)
		{
			ActiveEditorGizmo.Gizmo->Render(RenderAPI);
		}
	}
}

void UEditorInteractiveGizmoManager::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	Super::DrawHUD(Canvas, RenderAPI);

	if (GetShowEditorGizmosForView(RenderAPI))
	{
		for (FActiveEditorGizmo& ActiveEditorGizmo : ActiveEditorGizmos)
		{
			ActiveEditorGizmo.Gizmo->DrawHUD(Canvas, RenderAPI);
		}
	}
}

#undef LOCTEXT_NAMESPACE
