// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaInteractiveToolsToolBase.h"
#include "AvaInteractiveToolsSettings.h"
#include "AvaViewportUtils.h"
#include "AvalancheInteractiveToolsModule.h"
#include "BaseBehaviors/SingleClickBehavior.h"
#include "BaseBehaviors/SingleKeyCaptureBehavior.h"
#include "ContextObjectStore.h"
#include "EdMode/AvaInteractiveToolsEdMode.h"
#include "Editor/EditorEngine.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "EngineAnalytics.h"
#include "Framework/Application/SlateApplication.h"
#include "IAvaInteractiveToolsModeDetailsObject.h"
#include "IAvaInteractiveToolsModeDetailsObjectProvider.h"
#include "InteractiveToolManager.h"
#include "InteractiveToolsContext.h"
#include "Planners/AvaInteractiveToolsToolViewportAreaPlanner.h"
#include "Planners/AvaInteractiveToolsToolViewportPlanner.h"
#include "Planners/AvaInteractiveToolsToolViewportPointPlanner.h"
#include "ToolContextInterfaces.h"
#include "Toolkits/BaseToolkit.h"
#include "UnrealClient.h"
#include "ViewportClient/IAvaViewportClient.h"

#define LOCTEXT_NAMESPACE "AvaInteractiveToolsToolBase"

namespace UE::AvaInteractiveTools::Private
{
	static const FName AvaITFToolPresetMenuName = "AvaITFToolPresetMenu";
}

UAvaInteractiveToolsRightClickBehavior::UAvaInteractiveToolsRightClickBehavior()
{
	HitTestOnRelease = false;
	SetUseRightMouseButton();
}

void UAvaInteractiveToolsRightClickBehavior::Clicked(const FInputDeviceState& Input, const FInputCaptureData& Data)
{
	Target->OnUpdateModifierState(UAvaInteractiveToolsToolBase::BID_Cancel, true);
}

UObject* UAvaInteractiveToolsToolBase::GetDetailsObjectFromActor(AActor* InActor)
{
	if (!InActor)
	{
		return nullptr;
	}

	if (InActor->Implements<UAvaInteractiveToolsModeDetailsObject>())
	{
		return InActor;
	}
	else if (InActor->Implements<UAvaInteractiveToolsModeDetailsObjectProvider>())
	{
		return IAvaInteractiveToolsModeDetailsObjectProvider::Execute_GetModeDetailsObject(InActor);
	}

	TArray<UActorComponent*> Components;
	InActor->GetComponents<UActorComponent>(Components);

	for (UActorComponent* Component : Components)
	{
		if (Component->Implements<UAvaInteractiveToolsModeDetailsObject>())
		{
			return Component;
			break;
		}
		else if (Component->Implements<UAvaInteractiveToolsModeDetailsObjectProvider>())
		{
			if (UObject* DetailsObject = IAvaInteractiveToolsModeDetailsObjectProvider::Execute_GetModeDetailsObject(Component))
			{
				return DetailsObject;
			}
		}
	}

	return nullptr;
}

void UAvaInteractiveToolsToolBase::Setup()
{
	Super::Setup();

	UInteractiveToolManager* ToolManager = GetToolManager();
	bool bReactivated = false;
	UAvaInteractiveToolsEdMode* AvaInteractiveToolsEdMode = nullptr;

	if (UContextObjectStore* ContextStore = ToolManager->GetContextObjectStore())
	{
		AvaInteractiveToolsEdMode = Cast<UAvaInteractiveToolsEdMode>(ContextStore->FindContextByClass(UAvaInteractiveToolsEdMode::StaticClass()));

		if (AvaInteractiveToolsEdMode)
		{
			bReactivated = AvaInteractiveToolsEdMode->GetLastActiveTool() == ToolManager->GetActiveToolName(EToolSide::Left);
		}
	}

	if (!CanActivate(bReactivated))
	{
		ToolManager->PostActiveToolShutdownRequest(this, EToolShutdownType::Cancel);
	}

	if (AvaInteractiveToolsEdMode)
	{
		AvaInteractiveToolsEdMode->OnToolSetup(this);
	}

	Activate(bReactivated);
}

void UAvaInteractiveToolsToolBase::Shutdown(EToolShutdownType ShutdownType)
{
	switch (ShutdownType)
	{
		case EToolShutdownType::Cancel:
			OnCancel();
			break;

		case EToolShutdownType::Accept:
		case EToolShutdownType::Completed:
			OnComplete();
			break;
	}

	if (ViewportPlanner)
	{
		ViewportPlanner->Shutdown(ShutdownType);
		ViewportPlanner = nullptr;
	}

	Super::Shutdown(ShutdownType);
}

void UAvaInteractiveToolsToolBase::DrawHUD(FCanvas* InCanvas, IToolsContextRenderAPI* InRenderAPI)
{
	Super::DrawHUD(InCanvas, InRenderAPI);

	if (ViewportPlanner)
	{
		ViewportPlanner->DrawHUD(InCanvas, InRenderAPI);
	}
}

void UAvaInteractiveToolsToolBase::Render(IToolsContextRenderAPI* InRenderAPI)
{
	Super::Render(InRenderAPI);

	if (ViewportPlanner)
	{
		ViewportPlanner->Render(InRenderAPI);
	}
}

void UAvaInteractiveToolsToolBase::OnTick(float InDeltaTime)
{
	Super::OnTick(InDeltaTime);

	if (ViewportPlanner)
	{
		ViewportPlanner->OnTick(InDeltaTime);
	}
}

bool UAvaInteractiveToolsToolBase::SupportsDefaultAction() const
{
	return ViewportPlannerClass == UAvaInteractiveToolsToolViewportPointPlanner::StaticClass()
		|| ViewportPlannerClass == UAvaInteractiveToolsToolViewportAreaPlanner::StaticClass();
}

void UAvaInteractiveToolsToolBase::DefaultAction()
{
	RequestShutdown(EToolShutdownType::Completed);
}

bool UAvaInteractiveToolsToolBase::ConditionalIdentityRotation() const
{
	switch (GetDefault<UAvaInteractiveToolsSettings>()->DefaultActionActorAlignment)
	{
		case EAvaInteractiveToolsDefaultActionAlignment::Camera:
			if (!bPerformingDefaultAction)
			{
				return false;
			}

			return !IsMotionDesignViewport();

		default:
		case EAvaInteractiveToolsDefaultActionAlignment::Axis:
			return bPerformingDefaultAction;
	}
}

AActor* UAvaInteractiveToolsToolBase::SpawnActor(TSubclassOf<AActor> InActorClass, bool bInPreview, FString* InActorLabelOverride) const
{
	if (IToolsContextQueriesAPI* ContextAPI = GetToolManager()->GetContextQueriesAPI())
	{
		if (FViewport* Viewport = ContextAPI->GetFocusedViewport())
		{
			if (TSharedPtr<IAvaViewportClient> AvaViewportClient = FAvaViewportUtils::GetAvaViewportClient(Viewport))
			{
				const FVector2f ViewportSize = AvaViewportClient->GetViewportSize();
				return SpawnActor(InActorClass, EAvaViewportStatus::Focused, ViewportSize * 0.5, bInPreview, InActorLabelOverride);
			}
		}
	}

	return nullptr;
}

AActor* UAvaInteractiveToolsToolBase::SpawnActor(TSubclassOf<AActor> InActorClass, EAvaViewportStatus InViewportStatus, 
	const FVector2f& InViewportPosition, bool bInPreview, FString* InActorLabelOverride) const
{
	UWorld* World = nullptr;;
	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;
	if (IToolsContextQueriesAPI* ContextAPI = GetToolManager()->GetContextQueriesAPI())
	{
		const bool bUseIdentityLocation = UseIdentityLocation();
		const bool bUseIdentityRotation = UseIdentityRotation();

		if (FViewport* Viewport = ContextAPI->GetFocusedViewport())
		{
			FVector CameraForward = FVector(1, 0, 0);

			if (TSharedPtr<IAvaViewportClient> AvaViewportClient = FAvaViewportUtils::GetAvaViewportClient(Viewport))
			{
				CameraForward = AvaViewportClient->GetViewportViewTransform().TransformVectorNoScale(FVector::ForwardVector);
				CameraForward.Z = 0;
			}

			const bool bSuccess = ViewportPositionToWorldPositionAndOrientation(
				InViewportStatus,
				InViewportPosition,
				World,
				SpawnLocation,
				SpawnRotation
			);

			if (!bSuccess)
			{
				return nullptr;
			}

			if (bUseIdentityLocation)
			{
				SpawnLocation = FVector::ZeroVector;
			}

			if (bUseIdentityRotation)
			{
				if (CameraForward.Dot(FVector::ForwardVector) >= 0)
				{
					SpawnRotation = FRotator::ZeroRotator;
				}
				else
				{
					SpawnRotation = FRotator(0, 180, 0);
				}
			}
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.bNoFail = true;

	const bool bUseOverride = !bInPreview && InActorLabelOverride && !InActorLabelOverride->IsEmpty();

	if (bInPreview)
	{
		SpawnParams.bHideFromSceneOutliner = true;
		SpawnParams.bTemporaryEditorActor = true;
		SpawnParams.Name = FName(TEXT("AvaITFPreviewActor"));
		SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		SpawnParams.ObjectFlags |= RF_Transient;
	}
	else if (bUseOverride)
	{
		SpawnParams.Name = FName(*InActorLabelOverride);
		SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		SpawnParams.ObjectFlags |= RF_Transactional;
	}

	AActor* NewActor = World->SpawnActor<AActor>(InActorClass, SpawnLocation, SpawnRotation, SpawnParams);

	if (bUseOverride)
	{
		FActorLabelUtilities::SetActorLabelUnique(NewActor, *InActorLabelOverride);
	}
	else
	{
		FActorLabelUtilities::SetActorLabelUnique(NewActor, NewActor->GetDefaultActorLabel());
	}

	if (UObject* DetailsObject = GetDetailsObjectFromActor(NewActor))
	{
		SetToolkitSettingsObject(DetailsObject);
	}

	return NewActor;
}

void UAvaInteractiveToolsToolBase::SetToolkitSettingsObject(UObject* InObject) const
{
	if (UInteractiveToolManager* ToolManager = GetToolManager())
	{
		if (UContextObjectStore* ContextStore = ToolManager->GetContextObjectStore())
		{
			if (UAvaInteractiveToolsEdMode* AvaInteractiveToolsEdMode =
				Cast<UAvaInteractiveToolsEdMode>(ContextStore->FindContextByClass(UAvaInteractiveToolsEdMode::StaticClass())))
			{
				if (TSharedPtr<FModeToolkit> Toolkit = AvaInteractiveToolsEdMode->GetToolkit().Pin())
				{
					Toolkit->SetModeSettingsObject(InObject);
				}
			}
		}
	}
}

bool UAvaInteractiveToolsToolBase::ViewportPositionToWorldPositionAndOrientation(EAvaViewportStatus InViewportStatus, 
	const FVector2f& InViewportPosition, UWorld*& OutWorld, FVector& OutPosition, FRotator& OutRotation) const
{
	const float CameraDistance = GetDefault<UAvaInteractiveToolsSettings>()->CameraDistance;

	return ViewportPositionToWorldPositionAndOrientation(InViewportStatus, InViewportPosition, CameraDistance, OutWorld, OutPosition, OutRotation);
}

bool UAvaInteractiveToolsToolBase::ViewportPositionToWorldPositionAndOrientation(EAvaViewportStatus InViewportStatus,
	const FVector2f& InViewportPosition, float InDistance, UWorld*& OutWorld, FVector& OutPosition, FRotator& OutRotation) const
{
	if (IToolsContextQueriesAPI* ContextAPI = GetToolManager()->GetContextQueriesAPI())
	{
		if (UWorld* World = ContextAPI->GetCurrentEditingWorld())
		{
			if (TSharedPtr<IAvaViewportClient> AvaViewportClient = FAvaViewportUtils::GetAvaViewportClient(GetViewport(InViewportStatus)))
			{
				if (const FEditorViewportClient* EditorViewportClient = AvaViewportClient->AsEditorViewportClient())
				{
					const FViewportCameraTransform ViewTransform = EditorViewportClient->GetViewTransform();
					const FVector2f ViewportSize = AvaViewportClient->GetViewportSize();

					OutWorld = World;
					OutRotation = ViewTransform.GetRotation();
					OutPosition = AvaViewportClient->ViewportPositionToWorldPosition(InViewportPosition, InDistance);

					return true;
				}
			}
		}
	}

	return false;
}

FViewport* UAvaInteractiveToolsToolBase::GetViewport(EAvaViewportStatus InViewportStatus) const
{
	if (UInteractiveToolManager* ToolManager = GetToolManager())
	{
		if (IToolsContextQueriesAPI* ContextAPI = ToolManager->GetContextQueriesAPI())
		{
			FViewport* Viewport = nullptr;

			switch (InViewportStatus)
			{
				case EAvaViewportStatus::Hovered:
					Viewport = ContextAPI->GetHoveredViewport();
					break;

				case EAvaViewportStatus::Focused:
					Viewport = ContextAPI->GetFocusedViewport();
					break;

				default:
					checkNoEntry();
					return nullptr;
			}

			if (FAvaViewportUtils::GetAsEditorViewportClient(Viewport))
			{
				return Viewport;
			}
		}
	}

	return nullptr;
}

void UAvaInteractiveToolsToolBase::OnViewportPlannerComplete()
{
	if (PreviewActor)
	{
		PreviewActor->Destroy();
	}

	RequestShutdown(EToolShutdownType::Completed);
}

FInputRayHit UAvaInteractiveToolsToolBase::CanBeginSingleClickAndDragSequence(const FInputDeviceRay& InPressPos)
{
	// Always hits every place in the viewport
	return FInputRayHit(0);
}

void UAvaInteractiveToolsToolBase::OnClickPress(const FInputDeviceRay& InPressPos)
{
	// Nothing
}

void UAvaInteractiveToolsToolBase::OnDragStart(const FInputDeviceRay& InDragPos)
{
	if (LeftClickBehavior)
	{
		// Fake a click at the start position
		OnClickRelease(LeftClickBehavior->GetInitialMouseDownRay(), true);
	}
}

void UAvaInteractiveToolsToolBase::OnClickDrag(const FInputDeviceRay& InDragPos)
{
}

void UAvaInteractiveToolsToolBase::OnClickRelease(const FInputDeviceRay& InReleasePos, bool bInIsDragOperation)
{
	if (ViewportPlanner)
	{
		ViewportPlanner->OnClicked(InReleasePos);
	}
}

void UAvaInteractiveToolsToolBase::OnTerminateSingleClickAndDragSequence()
{
	RequestShutdown(EToolShutdownType::Cancel);
}

FInputRayHit UAvaInteractiveToolsToolBase::IsHitByClick(const FInputDeviceRay& InClickPos)
{
	return FInputRayHit(0);
}

void UAvaInteractiveToolsToolBase::OnClicked(const FInputDeviceRay& InClickPos)
{
	// Right click cancel
	RequestShutdown(EToolShutdownType::Cancel);
}

void UAvaInteractiveToolsToolBase::OnUpdateModifierState(int InModifierID, bool bInIsOn)
{
	switch (InModifierID)
	{
		case BID_Cancel:
			if (bInIsOn)
			{
				RequestShutdown(EToolShutdownType::Cancel);
			}

			break;
	}
}

bool UAvaInteractiveToolsToolBase::CanActivate(bool bInReactivate) const
{
	if (!bInReactivate)
	{

		return true;
	}

	return SupportsDefaultAction();
}

void UAvaInteractiveToolsToolBase::Activate(bool bInReactivate)
{
	if (!CanActivate(bInReactivate))
	{
		RequestShutdown(EToolShutdownType::Cancel);
		return;
	}

	FAvalancheInteractiveToolsModule::Get().OnToolActivated();

	const bool bUsePresetMenu = ShouldUsePresetMenu();
	const bool bSupportsDefaultAction = !bUsePresetMenu && SupportsDefaultAction();
	const bool bForceDefaultAction = !bUsePresetMenu && ShouldForceDefaultAction();

	if (bUsePresetMenu)
	{
		ShowPresetMenu();
	}
	else if (!bForceDefaultAction && (!bInReactivate || !bSupportsDefaultAction))
	{
		OnActivate();

		if (!OnBegin())
		{
			RequestShutdown(EToolShutdownType::Cancel);
			return;
		}

		OnPostBegin();
	}
	else if (bSupportsDefaultAction)
	{
		bPerformingDefaultAction = true;
		DefaultAction();
	}
	else
	{
		if (bForceDefaultAction)
		{
			UE_LOG(LogAvaInteractiveTools, Warning, TEXT("Alt used to force a tool into using the default action, but it does not support the default action."));
		}

		RequestShutdown(EToolShutdownType::Cancel);
	}
}

void UAvaInteractiveToolsToolBase::OnActivate()
{
}

bool UAvaInteractiveToolsToolBase::OnBegin()
{
	if (ViewportPlannerClass == nullptr)
	{
		return false;
	}

	BeginTransaction();
	return true;
}

void UAvaInteractiveToolsToolBase::OnPostBegin()
{
	LeftClickBehavior = NewObject<UAvaSingleClickAndDragBehavior>(this);
	LeftClickBehavior->Initialize(this);
	LeftClickBehavior->bSupportsDrag = ViewportPlannerClass.Get() && ViewportPlannerClass->IsChildOf<UAvaInteractiveToolsToolViewportAreaPlanner>();
	AddInputBehavior(LeftClickBehavior);

	RightClickBehavior = NewObject<UAvaInteractiveToolsRightClickBehavior>(this);
	RightClickBehavior->Initialize(this);
	AddInputBehavior(RightClickBehavior);

	EscapeKeyBehavior = NewObject<USingleKeyCaptureBehavior>(this);
	EscapeKeyBehavior->Initialize(static_cast<IClickBehaviorTarget*>(this), BID_Cancel, EKeys::Escape);
	AddInputBehavior(EscapeKeyBehavior);

	ViewportPlanner = NewObject<UAvaInteractiveToolsToolViewportPlanner>(this, ViewportPlannerClass);
	ViewportPlanner->Setup(this);
}

void UAvaInteractiveToolsToolBase::OnCancel()
{
	CancelTransaction();

	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}

	SetToolkitSettingsObject(nullptr);
}

void UAvaInteractiveToolsToolBase::OnComplete()
{
	EndTransaction();

	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes;
		Attributes.Emplace(TEXT("ToolClass"), GetClass()->GetName());
		if (SpawnedActor)
		{
			Attributes.Emplace(TEXT("ActorClass"), SpawnedActor->GetClass()->GetName());	
		}
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MotionDesign.CompleteTool"), Attributes);
	}
}

void UAvaInteractiveToolsToolBase::BeginTransaction()
{
	GetToolManager()->BeginUndoTransaction(LOCTEXT("MotionDesignInteractiveToolsTool", "Motion Design Interactive Tools Box Tool"));
}

void UAvaInteractiveToolsToolBase::EndTransaction()
{
	GetToolManager()->EndUndoTransaction();
}

void UAvaInteractiveToolsToolBase::CancelTransaction()
{
	// Doesn't exist
	//GetToolManager()->CancelUndoTransaction();
	GetToolManager()->EndUndoTransaction();
}

void UAvaInteractiveToolsToolBase::RequestShutdown(EToolShutdownType InShutdownType)
{
	if (UContextObjectStore* ContextStore = GetToolManager()->GetContextObjectStore())
	{
		if (UAvaInteractiveToolsEdMode* AvaInteractiveToolsEdMode = Cast<UAvaInteractiveToolsEdMode>(ContextStore->FindContextByClass(UAvaInteractiveToolsEdMode::StaticClass())))
		{
			AvaInteractiveToolsEdMode->OnToolShutdown(this, InShutdownType);
		}
	}

	if (ViewportPlanner)
	{
		ViewportPlanner->Shutdown(InShutdownType);
	}

	SetToolkitSettingsObject(nullptr);

	GetToolManager()->PostActiveToolShutdownRequest(this, InShutdownType);

	FAvalancheInteractiveToolsModule::Get().OnToolDeactivated();
}

bool UAvaInteractiveToolsToolBase::IsMotionDesignViewport() const
{
	if (IToolsContextQueriesAPI* ContextAPI = GetToolManager()->GetContextQueriesAPI())
	{
		if (FViewport* Viewport = ContextAPI->GetFocusedViewport())
		{
			if (TSharedPtr<IAvaViewportClient> AvaViewportClient = FAvaViewportUtils::GetAvaViewportClient(Viewport))
			{
				return AvaViewportClient->IsMotionDesignViewport();
			}
		}
	}

	return false;
}

bool UAvaInteractiveToolsToolBase::ShouldForceDefaultAction() const
{
	return FSlateApplication::Get().GetModifierKeys().IsAltDown();
}

bool UAvaInteractiveToolsToolBase::ShouldUsePresetMenu() const
{
	const FAvaInteractiveToolsToolParameters ToolParams = GetToolParameters();

	if (ToolParams.Presets.IsEmpty())
	{
		return false;
	}

	return SupportsDefaultAction() && FSlateApplication::Get().GetModifierKeys().IsShiftDown();
}

void UAvaInteractiveToolsToolBase::ShowPresetMenu()
{
	using namespace UE::AvaInteractiveTools::Private;

	RegisterPresetMenu();

	TSharedPtr<SWidget> Parent = FSlateApplication::Get().FindBestParentWindowForDialogs(nullptr);

	if (!Parent.IsValid())
	{
		return;
	}

	FToolMenuContext Context;
	Context.AddObject(this);

	FSlateApplication::Get().PushMenu(
		Parent.ToSharedRef(),
		FWidgetPath(),
		UToolMenus::Get()->GenerateWidget(AvaITFToolPresetMenuName, Context),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect::ContextMenu
	);
}

void UAvaInteractiveToolsToolBase::OnPresetSelected(TStrongObjectPtr<UAvaInteractiveToolsToolBase> InToolPtr, FName InPresetName)
{
	UAvaInteractiveToolsToolBase* Tool = InToolPtr.Get();

	if (!Tool)
	{
		return;
	}

	const FAvaInteractiveToolsToolParameters ToolParams = Tool->GetToolParameters();
	const TSharedRef<FAvaInteractiveToolsToolPresetBase>* Preset = ToolParams.Presets.Find(InPresetName);

	if (!Preset)
	{
		return;
	}

	Tool->bPerformingDefaultAction = true;
	Tool->DefaultAction();

	(*Preset)->ApplyPreset(Tool->GetSpawnedActor());
}

void UAvaInteractiveToolsToolBase::RegisterPresetMenu()
{
	using namespace UE::AvaInteractiveTools::Private;

	UToolMenus* ToolMenus = UToolMenus::Get();

	if (ToolMenus->IsMenuRegistered(AvaITFToolPresetMenuName))
	{
		return;
	}

	UToolMenu* PresetMenu = ToolMenus->RegisterMenu(AvaITFToolPresetMenuName);

	static const FName PresetSectionName = "Preset";

	PresetMenu->AddDynamicSection(
		PresetSectionName,
		FNewSectionConstructChoice(FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* InMenu)
			{
				UAvaInteractiveToolsToolBase* Tool = InMenu->FindContext<UAvaInteractiveToolsToolBase>();

				if (!Tool)
				{
					return;
				}

				const FAvaInteractiveToolsToolParameters ToolParams = Tool->GetToolParameters();

				if (ToolParams.Presets.IsEmpty())
				{
					return;
				}

				TStrongObjectPtr<UAvaInteractiveToolsToolBase> ToolPtr = TStrongObjectPtr<UAvaInteractiveToolsToolBase>(Tool);
				FToolMenuSection& Section = InMenu->FindOrAddSection("Presets");

				for (const TPair<FName, TSharedRef<FAvaInteractiveToolsToolPresetBase>>& PresetPair : ToolParams.Presets)
				{
					Section.AddMenuEntry(
						PresetPair.Key,
						PresetPair.Value->GetName(),
						PresetPair.Value->GetDescription(),
						ToolParams.UICommand->GetIcon(),
						FToolUIActionChoice(FExecuteAction::CreateStatic(&UAvaInteractiveToolsToolBase::OnPresetSelected, ToolPtr, PresetPair.Key))
					);
				}
			}
		))
	);
}

#undef LOCTEXT_NAMESPACE
