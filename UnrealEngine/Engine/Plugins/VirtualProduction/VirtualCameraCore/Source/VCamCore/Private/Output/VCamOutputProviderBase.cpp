// Copyright Epic Games, Inc. All Rights Reserved.

#include "Output/VCamOutputProviderBase.h"

#include "Interface/IVCamOutputProviderCreatedWidget.h"
#include "UI/VCamWidget.h"
#include "Util/LevelViewportUtils.h"
#include "Util/ObjectMessageAggregation.h"
#include "Util/WidgetSnapshotUtils.h"
#include "Util/WidgetTreeUtils.h"
#include "VCamComponent.h"
#include "VCamCoreCustomVersion.h"
#include "Output/ViewTargetPolicy/FocusFirstPlayerViewTargetPolicy.h"
#include "Output/ViewTargetPolicy/GameplayViewTargetPolicy.h"

#include "Algo/RemoveIf.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"
#include "SceneViewExtensionContext.h"
#include "Slate/SceneViewport.h"
#include "UObject/UObjectBaseUtility.h"
#include "Util/BlueprintUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#include "IAssetViewport.h"
#include "LevelEditorViewport.h"
#include "SLevelViewport.h"
#include "SEditorViewport.h"
#include "UnrealClient.h"
#endif

DEFINE_LOG_CATEGORY(LogVCamOutputProvider);

#define LOCTEXT_NAMESPACE "UVCamOutputProviderBase"

namespace UE::VCamCore::Private
{
	static bool ValidateOverlayClassAndLogErrors(const TSubclassOf<UUserWidget>& InUMGClass)
	{
		// Null IS allowed and means "do not create any class"
		const bool bHasCorrectClassFlags = !InUMGClass || !InUMGClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated);
		UE_CLOG(!bHasCorrectClassFlags, LogVCamOutputProvider, Warning, TEXT("Class %s cannot be deprecated nor abstract"), *InUMGClass->GetPathName());
		return bHasCorrectClassFlags;
	}
}

UVCamOutputProviderBase::UVCamOutputProviderBase()
{
	OnActivatedDelegate.AddLambda([this](bool bNewValue)
	{
		OnActivatedDelegate_Blueprint.Broadcast(bNewValue);
	});
	
	GameplayViewTargetPolicy = CreateDefaultSubobject<UFocusFirstPlayerViewTargetPolicy>(TEXT("FocusFirstPlayerViewTargetPolicy0"));
}

void UVCamOutputProviderBase::BeginDestroy()
{
	Deinitialize();
	Super::BeginDestroy();
}

void UVCamOutputProviderBase::Initialize()
{
	bool bWasInitialized = bInitialized;
	bInitialized = true;

	// Reactivate the provider if it was previously set to active
	if (!bWasInitialized && bIsActive)
	{
#if WITH_EDITOR
		// If the editor viewports aren't fully initialized, then delay initialization for the entire Output Provider
		if (GEditor && GEditor->GetActiveViewport() && (GEditor->GetActiveViewport()->GetSizeXY().X < 1))
		{
			bInitialized = false;
		}
		else
#endif
		{
			if (IsOuterComponentEnabledAndInitialized())
			{
				OnActivate();
			}
		}
	}
}

void UVCamOutputProviderBase::Deinitialize()
{
	if (bInitialized)
	{
		OnDeactivate();
		bInitialized = false;
	}
}

void UVCamOutputProviderBase::Tick(const float DeltaTime)
{
	if (bIsActive && UMGWidget && UMGClass)
	{
		UMGWidget->Tick(DeltaTime);
	}
}

void UVCamOutputProviderBase::SetActive(const bool bInActive)
{
	bIsActive = bInActive;
	
	// E.g. when you drag-drop an actor into the level
	if (!UE::VCamCore::CanInitVCamOutputProvider(this))
	{
		return;
	}

	// Deactivation is a clean up operation that we always allow but ...
	if (!bIsActive)
	{
		OnDeactivate();
		return;
	}
	
	// ... we enforce that in OnActivate that we be initialized first.
	// For the VCam connections & modifiers to work with the output widget, the VCamComponent must be initialized.
	// If this output provider is !bInitialized, the most likely reason is that the owning VCam component is also not initialized.
	const bool bCanPerformActivationLogic = bInitialized && IsOuterComponentEnabledAndInitialized();
	if (bCanPerformActivationLogic)
	{
		OnActivate();
	}
	else
	{
		// ... and instead of resolving that here, we defer to the API user to resolve the issue by initializing the owning VCamComponent, e.g. with a SetEnabled(true) call. 
		UE_LOG(LogVCamOutputProvider,
			Warning,
			TEXT("SetActive: Owning VCamComponent is not enabled or initialized. Call SetEnabled(true) on the owning VCamComponent. Output provider bIsActive was set to true but the activation logic was skipped; it will run once you initialize the owning VCamComponent. Output provider: %s"),
			*GetPathName()
			);
	}
}

bool UVCamOutputProviderBase::IsOuterComponentEnabledAndInitialized() const
{
	const UVCamComponent* OuterComponent = GetTypedOuter<UVCamComponent>();
	return OuterComponent && OuterComponent->IsEnabled() && OuterComponent->IsInitialized();
}

void UVCamOutputProviderBase::SetTargetViewport(EVCamTargetViewportID Value)
{
	TargetViewport = Value;
	ReinitializeViewportIfNeeded();
}

void UVCamOutputProviderBase::SetUMGClass(const TSubclassOf<UUserWidget> InUMGClass)
{
	if (UE::VCamCore::Private::ValidateOverlayClassAndLogErrors(InUMGClass))
	{
		UMGClass = InUMGClass;
	}
}

UVCamComponent* UVCamOutputProviderBase::GetVCamComponent() const
{
	return GetTypedOuter<UVCamComponent>();
}

void UVCamOutputProviderBase::ReapplyOverrideResolution()
{
	if (!IsActiveAndOuterComponentAllowsActivity())
	{
		return;
	}
	
	if (bUseOverrideResolution)
	{
		ApplyOverrideResolutionForViewport(TargetViewport);
	}
	else
	{
		RestoreOverrideResolutionForViewport(TargetViewport);
	}
}
void UVCamOutputProviderBase::OnActivate()
{
	check(IsInitialized());

	ConditionallySetUpGameplayViewTargets();
	ReapplyOverrideResolution();
	
	CreateUMG();
	DisplayUMG();

	OnActivatedDelegate.Broadcast(true);
}

void UVCamOutputProviderBase::OnDeactivate()
{
	ConditionallyCleanUpGameplayViewTargets();
	if (bUseOverrideResolution)
	{
		RestoreOverrideResolutionForViewport(TargetViewport);
	}
	
	DestroyUMG();
	
	OnActivatedDelegate.Broadcast(false);
}

void UVCamOutputProviderBase::CreateUMG()
{
	if (!UMGClass)
	{
		return;
	}

	if (UMGWidget)
	{
		UE_LOG(LogVCamOutputProvider, Error, TEXT("CreateUMG widget already set - failed to create"));
		return;
	}

	// Warn the user if the viewport is not available ...
	const TSharedPtr<FSceneViewport> Viewport = GetSceneViewport(TargetViewport);
	if (!Viewport)
	{
		DisplayNotification_ViewportNotFound();
		return;
	}

	UMGWidget = NewObject<UVPFullScreenUserWidget>(this, UVPFullScreenUserWidget::StaticClass());
	UMGWidget->SetDisplayTypes(DisplayType, DisplayType, DisplayType);
	if (UMGWidget->DoesDisplayTypeUsePostProcessSettings(DisplayType))
	{
		UMGWidget->GetPostProcessDisplayTypeSettingsFor(DisplayType)->bReceiveHardwareInput = true;
	}

#if WITH_EDITOR
	// Only register in editor because editor has multiple viewports. In games, there is only one viewport (ignore split screen).
	if (UMGWidget->GetDisplayType(GetWorld()) == EVPWidgetDisplayType::PostProcessSceneViewExtension)
	{
		FSceneViewExtensionIsActiveFunctor IsActiveFunctor;
		IsActiveFunctor.IsActiveFunction = [WeakThis = TWeakObjectPtr<UVCamOutputProviderBase>(this)](const ISceneViewExtension* SceneViewExtension, const FSceneViewExtensionContext& Context) 
		{
			if (WeakThis.IsValid())
			{
				return WeakThis->GetRenderWidgetStateInContext(SceneViewExtension, Context);
			}
			return TOptional<bool>{};
		};
		UMGWidget->GetPostProcessDisplayTypeWithSceneViewExtensionsSettings().RegisterIsActiveFunctor(MoveTemp(IsActiveFunctor));
	}
	
	UMGWidget->SetEditorTargetViewport(Viewport);
#endif

	UMGWidget->SetWidgetClass(UMGClass);
	UE_LOG(LogVCamOutputProvider, Log, TEXT("CreateUMG widget named %s from class %s"), *UMGWidget->GetName(), *UMGWidget->GetWidgetClass()->GetName());
}

void UVCamOutputProviderBase::DisplayUMG()
{
	if (UMGWidget)
	{
		if (UWorld* ActorWorld = GetWorld())
		{
#if WITH_EDITOR
			// In the editor, we override the target viewport's post process settings instead of using the cine camera
			// because other output providers with post process output would interfer with each other...
			UMGWidget->SetCustomPostProcessSettingsSource(this);

			FLevelEditorViewportClient* Client = GetTargetLevelViewportClient();
			UE_CLOG(DisplayType == EVPWidgetDisplayType::PostProcessWithBlendMaterial && !Client, LogVCamOutputProvider, Error, TEXT("Failed to find viewport client. The widget will not be rendered."));
			if (DisplayType == EVPWidgetDisplayType::PostProcessWithBlendMaterial && Client)
			{
				ensure(!ModifyViewportPostProcessSettingsDelegateHandle.IsValid());
				ModifyViewportPostProcessSettingsDelegateHandle = Client->ViewModifiers.AddUObject(this, &UVCamOutputProviderBase::ModifyViewportPostProcessSettings);
				Client->bShouldApplyViewModifiers = true;
			}
#else
			// ... but in games there is only one viewport so we can just use the cine camera.
			UMGWidget->SetCustomPostProcessSettingsSource(TargetCamera.Get());
#endif

#if WITH_EDITOR
			// Creating widgets should not be transacted because it will create a huge transaction.
			ITransaction* UndoState = GUndo;
			GUndo = nullptr;
			ON_SCOPE_EXIT{ GUndo = UndoState; };
#endif
			if (!UMGWidget->Display(ActorWorld)
				|| !ensureAlwaysMsgf(UMGWidget->GetWidget(), TEXT("UVPFullScreenUserWidget::Display returned true but did not create any subwidget!")))
			{
				return;
			}

			if (WidgetSnapshot.HasData())
			{
				UE::VCamCore::WidgetSnapshotUtils::Private::ApplyTreeHierarchySnapshot(WidgetSnapshot, *UMGWidget->GetWidget());
				// Note that NotifyWidgetOfComponentChange will cause InitializeConnections to be called - this is important for the connections to get applied!
			}
		}

		NotifyAboutComponentChange();
#if WITH_EDITOR
		// Start registering after the initial calls to InitializeConnections to prevent unneeded snapshotting.
		StartDetectAndSnapshotWhenConnectionsChange();
#endif
	}
}

void UVCamOutputProviderBase::DestroyUMG()
{
	if (UMGWidget)
	{
		if (UMGWidget->IsDisplayed())
		{
#if WITH_EDITOR
			// The state only needs to be saved in the editor
			if (UUserWidget* Subwidget = UMGWidget->GetWidget(); ensure(Subwidget))
			{
				StopDetectAndSnapshotWhenConnectionsChange();
				Modify();
				WidgetSnapshot = UE::VCamCore::WidgetSnapshotUtils::Private::TakeTreeHierarchySnapshot(*Subwidget);
			}

			FLevelEditorViewportClient* Client = GetTargetLevelViewportClient();
			if (DisplayType == EVPWidgetDisplayType::PostProcessWithBlendMaterial && Client && ModifyViewportPostProcessSettingsDelegateHandle.IsValid())
			{
				Client->ViewModifiers.Remove(ModifyViewportPostProcessSettingsDelegateHandle);
				Client->bShouldApplyViewModifiers = Client->ViewModifiers.IsBound();
			}
			ModifyViewportPostProcessSettingsDelegateHandle.Reset();
#endif
			
			UMGWidget->Hide();
			UE_LOG(LogVCamOutputProvider, Log, TEXT("DestroyUMG widget %s hidden"), *UMGWidget->GetName());
		}
		UE_LOG(LogVCamOutputProvider, Log, TEXT("DestroyUMG widget %s destroyed"), *UMGWidget->GetName());

#if WITH_EDITOR
		UMGWidget->ResetEditorTargetViewport();
#endif

		UMGWidget->ConditionalBeginDestroy();
		UMGWidget = nullptr;
	}
}

void UVCamOutputProviderBase::DisplayNotification_ViewportNotFound() const
{
#if WITH_EDITOR
	// Only show if not undoing because the user was already shown the message before.
	// Also not while replaying transactions via Multi User since old transactions in the chain should not trigger this message.
	if (bIsUndoing)
	{
		return;
	}
#endif
	
	AActor* OwningActor = GetTypedOuter<AActor>();
	check(OwningActor);
		
	using namespace UE::VCamCore;
	const FString ActorName =
#if WITH_EDITOR
		OwningActor->GetActorLabel();
#else
			OwningActor->GetPathName();
#endif
	AddAggregatedNotification(*OwningActor,
		{
			NotificationKey_MissingTargetViewport,
			FText::Format(LOCTEXT("MissingTargetViewport.Title", "Missing target viewport: {0}"), FText::FromString(ActorName)),
			FText::Format(LOCTEXT("MissingTargetViewport.Subtext", "Edit output provider {1} or open {0} (Window > Viewports)."), FText::FromString(ViewportIdToString(TargetViewport)), FindOwnIndexInOwner())
		});
}

void UVCamOutputProviderBase::OnSetTargetCamera(const UCineCameraComponent* InTargetCamera)
{
	if (InTargetCamera != TargetCamera)
	{
		TargetCamera = InTargetCamera;
		NotifyAboutComponentChange();
	}
}

void UVCamOutputProviderBase::RestoreOverrideResolutionForViewport(EVCamTargetViewportID ViewportToRestore)
{
	if (const TSharedPtr<FSceneViewport> TargetSceneViewport = GetSceneViewport(ViewportToRestore))
	{
		TargetSceneViewport->SetFixedViewportSize(0, 0);
	}
}

void UVCamOutputProviderBase::ApplyOverrideResolutionForViewport(EVCamTargetViewportID Viewport)
{
	if (const TSharedPtr<FSceneViewport> TargetSceneViewport = GetSceneViewport(Viewport))
	{
		TargetSceneViewport->SetFixedViewportSize(OverrideResolution.X, OverrideResolution.Y);
	}
}

void UVCamOutputProviderBase::SuspendOutput()
{
	if (IsActive())
	{
		bWasOutputSuspendedWhileActive = true;
		SetActive(false);
	}
}

void UVCamOutputProviderBase::RestoreOutput()
{
	if (bWasOutputSuspendedWhileActive && !IsActive())
	{
		SetActive(true);
	}
	bWasOutputSuspendedWhileActive = false;
}

bool UVCamOutputProviderBase::NeedsForceLockToViewport() const
{
	// The widget is displayed via a post process material, which is applied to the camera's post process settings, hence anything will only be visible when locked.
	return DisplayType == EVPWidgetDisplayType::PostProcessWithBlendMaterial || DisplayType == EVPWidgetDisplayType::Composure;
}

void UVCamOutputProviderBase::NotifyAboutComponentChange()
{
	if (UMGWidget && UMGWidget->IsDisplayed())
	{
		UUserWidget* DisplayedWidget = UMGWidget->GetWidget();
		if (IsValid(DisplayedWidget))
		{
			if (UVCamComponent* OwningComponent = GetTypedOuter<UVCamComponent>())
			{
				UVCamComponent* VCamComponent = bIsActive ? OwningComponent : nullptr;
				
				// Find all VCam Widgets inside the displayed widget and Initialize them with the owning VCam Component
				UE::VCamCore::ForEachWidgetToConsiderForVCam(*DisplayedWidget, [this, VCamComponent](UWidget* Widget)
				{
					if (UVCamWidget* VCamWidget = Cast<UVCamWidget>(Widget))
					{
						VCamWidget->InitializeConnections(VCamComponent);
					}
					
					if (Widget->Implements<UVCamOutputProviderCreatedWidget>())
					{
						IVCamOutputProviderCreatedWidget::Execute_ReceiveOutputProvider(Widget, FVCamReceiveOutputProviderData{ this });
					}
				});
			}
		}
	}
}

UVCamOutputProviderBase* UVCamOutputProviderBase::GetOtherOutputProviderByIndex(int32 Index) const
{
	if (Index > INDEX_NONE)
	{
		if (const UVCamComponent* OuterComponent = GetTypedOuter<UVCamComponent>())
		{
			if (UVCamOutputProviderBase* Provider = OuterComponent->GetOutputProviderByIndex(Index))
			{
				return Provider;
			}
			
			UE_LOG(LogVCamOutputProvider, Warning, TEXT("GetOtherOutputProviderByIndex - specified index is out of range"));
		}
	}

	return nullptr;
}

int32 UVCamOutputProviderBase::FindOwnIndexInOwner() const
{
	if (const UVCamComponent* OuterComponent = GetTypedOuter<UVCamComponent>())
	{
		for (int32 Index = 0; Index < OuterComponent->GetNumberOfOutputProviders(); ++Index)
		{
			if (OuterComponent->GetOutputProviderByIndex(Index) == this)
			{
				return Index;
			}
		}
	}
	return INDEX_NONE;
}

void UVCamOutputProviderBase::Serialize(FArchive& Ar)
{
	using namespace UE::VCamCore;
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FVCamCoreCustomVersion::GUID);
	
	if (Ar.IsLoading() && Ar.CustomVer(FVCamCoreCustomVersion::GUID) < FVCamCoreCustomVersion::MoveTargetViewportFromComponentToOutput)
	{
		const UVCamComponent* OuterComponent = GetTypedOuter<UVCamComponent>();
		TargetViewport = OuterComponent
			? OuterComponent->TargetViewport_DEPRECATED
			: TargetViewport;
	}
}

void UVCamOutputProviderBase::PostLoad()
{
	Super::PostLoad();

	// Class may have been marked deprecated or abstract since the last time it was set
	if (!UE::VCamCore::Private::ValidateOverlayClassAndLogErrors(UMGClass))
	{
		Modify();
		SetUMGClass(nullptr);
	}
}

#if WITH_EDITOR

void UVCamOutputProviderBase::PreEditUndo()
{
	bIsUndoing = true;
	Super::PreEditUndo();

	if (UE::VCamCore::CanInitVCamOutputProvider(this))
	{
		// If bIsActive is about to be set to false, we need to deactivate here because either
		// - UMGWidget will be null-ed, or
		// - the UVPFullScreenWidget::CurrentDisplayType will be set to Inactive
		// Both prevent us from removing the widget from the viewport correctly so we'll just ALWAYS disable and optionally restore in PostEditUndo.
		OnDeactivate();
	}
}

void UVCamOutputProviderBase::PostEditUndo()
{
	ON_SCOPE_EXIT { bIsUndoing = false; };
	Super::PostEditUndo();

	if (UE::VCamCore::CanInitVCamOutputProvider(this) && IsActiveAndOuterComponentAllowsActivity())
	{
		// Need to restore because we killed the widget in PreEditUndo
		// The transaction has overwritten our properties, e.g. UMGWidget, which would make OnActivate fail 
		OnDeactivate();

		// Our initialized state may also not line up anymore - in that case we must be initialized before activating.
		if (!IsInitialized())
		{
			Initialize();
		}
		
		// Now we're in a clean base state to re-activate
		OnActivate();
	}
}

void UVCamOutputProviderBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FProperty* Property = PropertyChangedEvent.MemberProperty;
	if (UE::VCamCore::CanInitVCamOutputProvider(this)
		&& Property
		&& PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		static FName NAME_IsActive = GET_MEMBER_NAME_CHECKED(UVCamOutputProviderBase, bIsActive);
		static FName NAME_UMGClass = GET_MEMBER_NAME_CHECKED(UVCamOutputProviderBase, UMGClass);
		static FName NAME_TargetViewport = GET_MEMBER_NAME_CHECKED(UVCamOutputProviderBase, TargetViewport);
		static FName NAME_OverrideResolution = GET_MEMBER_NAME_CHECKED(UVCamOutputProviderBase, OverrideResolution);
		static FName NAME_bUseOverrideResolution = GET_MEMBER_NAME_CHECKED(UVCamOutputProviderBase, bUseOverrideResolution);

		const FName PropertyName = Property->GetFName();
		if (PropertyName == NAME_IsActive)
		{
			SetActive(bIsActive);
		}
		else if (PropertyName == NAME_UMGClass)
		{
			WidgetSnapshot.Reset();
			if (IsActiveAndOuterComponentAllowsActivity())
			{
				// In case a child class resets UMGClass, reapply the correct value we got the PostEditChangeProperty for.
				const TSubclassOf<UUserWidget> ProtectUMGClass = UMGClass;
				SetActive(false);
				// Does additional checks; Unreal Editor already ensures we do not get deprecated / abstract classes but we may add more checks in future.
				SetUMGClass(ProtectUMGClass);
				SetActive(true);
			}
		}
		else if (PropertyName == NAME_TargetViewport)
		{
			ReinitializeViewportIfNeeded();
		}
		else if (PropertyName == NAME_OverrideResolution || PropertyName == NAME_bUseOverrideResolution)
		{
			ReapplyOverrideResolution();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

TSharedPtr<FSceneViewport> UVCamOutputProviderBase::GetSceneViewport(EVCamTargetViewportID InTargetViewport) const
{
	TSharedPtr<FSceneViewport> SceneViewport;
	
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				FSlatePlayInEditorInfo* SlatePlayInEditorSession = GEditor->SlatePlayInEditorMap.Find(Context.ContextHandle);
				if (SlatePlayInEditorSession)
				{
					if (SlatePlayInEditorSession->DestinationSlateViewport.IsValid())
					{
						TSharedPtr<IAssetViewport> DestinationLevelViewport = SlatePlayInEditorSession->DestinationSlateViewport.Pin();
						return DestinationLevelViewport->GetSharedActiveViewport();
					}
					if (SlatePlayInEditorSession->SlatePlayInEditorWindowViewport.IsValid())
					{
						return SlatePlayInEditorSession->SlatePlayInEditorWindowViewport;
					}
				}
			}
			else if (Context.WorldType == EWorldType::Editor)
			{
				TSharedPtr<SLevelViewport> Viewport = UE::VCamCore::LevelViewportUtils::Private::GetLevelViewport(InTargetViewport);
				FLevelEditorViewportClient* LevelViewportClient = Viewport
					? &Viewport->GetLevelViewportClient()
					: nullptr;
				if (LevelViewportClient)
				{
					TSharedPtr<SEditorViewport> ViewportWidget = LevelViewportClient->GetEditorViewportWidget();
					if (ViewportWidget.IsValid())
					{
						SceneViewport = ViewportWidget->GetSceneViewport();
					}
				}
			}
		}
	}
#endif
	
	// Prefer returning the game viewport whenever it is available, such as when we launch in Standalone mode.
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	return GameEngine ? GameEngine->SceneViewport : SceneViewport;
}

TWeakPtr<SWindow> UVCamOutputProviderBase::GetTargetInputWindow() const
{
	TWeakPtr<SWindow> InputWindow;

#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				FSlatePlayInEditorInfo* SlatePlayInEditorSession = GEditor->SlatePlayInEditorMap.Find(Context.ContextHandle);
				
				if (SlatePlayInEditorSession && SlatePlayInEditorSession->DestinationSlateViewport.IsValid())
				{
					TSharedPtr<IAssetViewport> DestinationLevelViewport = SlatePlayInEditorSession->DestinationSlateViewport.Pin();
					return FSlateApplication::Get().FindWidgetWindow(DestinationLevelViewport->AsWidget());
				}
				if (SlatePlayInEditorSession && SlatePlayInEditorSession->SlatePlayInEditorWindowViewport.IsValid())
				{
					return SlatePlayInEditorSession->SlatePlayInEditorWindow;
				}
			}
			else if (Context.WorldType == EWorldType::Editor)
			{
				if (FLevelEditorViewportClient* LevelViewportClient = GetTargetLevelViewportClient())
				{
					TSharedPtr<SEditorViewport> ViewportWidget = LevelViewportClient->GetEditorViewportWidget();
					if (ViewportWidget.IsValid())
					{
						InputWindow = FSlateApplication::Get().FindWidgetWindow(ViewportWidget.ToSharedRef());
					}
				}
			}
		}
	}
#endif
	
	// Prefer returning the game viewport whenever it is available, such as when we launch in Standalone mode.
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	return GameEngine ? GameEngine->GameViewportWindow : InputWindow;
}

#if WITH_EDITOR

FLevelEditorViewportClient* UVCamOutputProviderBase::GetTargetLevelViewportClient() const
{
	const TSharedPtr<SLevelViewport> LevelViewport = GetTargetLevelViewport();
	return LevelViewport
		? &LevelViewport->GetLevelViewportClient()
		: nullptr;
}

TSharedPtr<SLevelViewport> UVCamOutputProviderBase::GetTargetLevelViewport() const
{
	return UE::VCamCore::LevelViewportUtils::Private::GetLevelViewport(TargetViewport);
}

#endif

void UVCamOutputProviderBase::ReinitializeViewportIfNeeded()
{
	for (int32 i = 0; i < static_cast<int32>(EVCamTargetViewportID::Count); ++i)
	{
		RestoreOverrideResolutionForViewport(static_cast<EVCamTargetViewportID>(i));
	}
	ReapplyOverrideResolution();

	const TSharedPtr<FSceneViewport> Viewport = GetSceneViewport(TargetViewport);
	if (!Viewport)
	{
		DisplayNotification_ViewportNotFound();
		return;
	}
	
	if (IsOutputting())
	{
		ReinitializeViewport();
	}
}

void UVCamOutputProviderBase::ReinitializeViewport()
{
	// This new flow is introduced with 5.4.
	// Before 5.4, changing the target viewport would reinitialize the output provider with the below SetActive(false) SetActive(true) flow.
	// This is undesirable because SetActive(false) kills current resources, like a connection to an external device (e.g. pixel stream), and then re-initializes them with the new target settings in SetActive(true).
	// Starting 5.4, we give the output provider the option to rebind the viewport's UMG widget dynamically.
	// So e.g. instead of killing the pixel stream and then starting it fully up again the underlying streamed buffer would be changed.

	// However, this new flow needs to be supported by the user output providers. When 5.4 goes out, users have obviously not implemented the new flow yet so we must stay backwards compatible.
	// We use PreReapplyViewport to inform and ask the implementation whether the new flow is supported.
	// By default, PreReapplyViewport returns EViewportChangeReply::Reinitialize, which causes us to run the same logic that happened before 5.4.
	// If EViewportChangeReply::ApplyViewportChange is returned, the output provider acknowledges support for the dynamic change and performs it in PostReapplyViewport.
	
	const UE::VCamCore::EViewportChangeReply ViewportChangeReply = PreReapplyViewport();
	if (ViewportChangeReply != UE::VCamCore::EViewportChangeReply::ApplyViewportChange)
	{
		// Backwards compatible path:
		// Pre 5.4, output providers would fully reinitialize everything like this.
		SetActive(false);
		SetActive(true);
		return;
	}

	// 5.4 dynamic path:
	DestroyUMG();
	CreateUMG();
	DisplayUMG();

	// Implementation will now rebind the outputting resources to the buffers of the new target viewport:
	PostReapplyViewport();
}

#if WITH_EDITOR

void UVCamOutputProviderBase::ModifyViewportPostProcessSettings(FEditorViewportViewModifierParams& EditorViewportViewModifierParams)
{
	// The UMGWidget has put a post process material into PostProcessSettingsForWidget which causes the widget to be rendered 
	EditorViewportViewModifierParams.AddPostProcessBlend(PostProcessSettingsForWidget, 1.f);
}

TOptional<bool> UVCamOutputProviderBase::GetRenderWidgetStateInContext(const ISceneViewExtension* SceneViewExtension, const FSceneViewExtensionContext& Context)
{
	const bool bWillRenderIntoTargetViewport = Context.Viewport && GetTargetLevelViewportClient() == Context.Viewport->GetClient();
	const bool bIsGameWorld = GetWorld()->IsGameWorld();
	return bWillRenderIntoTargetViewport
		// Always allow rendering into game worlds
		|| bIsGameWorld
		// By contract we should only ever return false when it is not ok to render and return empty otherwise
		? TOptional<bool>{}
		: false;
}

void UVCamOutputProviderBase::StartDetectAndSnapshotWhenConnectionsChange()
{
	checkf(UMGWidget, TEXT("Should only be called as part of UMGWidget initialization"));
	UUserWidget* Subwidget = UMGWidget->GetWidget();
	check(Subwidget);
	
	UE::VCamCore::ForEachWidgetToConsiderForVCam(*Subwidget, [this](UWidget* Widget)
	{
		if (UVCamWidget* VCamWidget = Cast<UVCamWidget>(Widget))
		{
			const TWeakObjectPtr<UVCamWidget> WeakWidget = VCamWidget;
			VCamWidget->OnPostConnectionsReinitializedDelegate.AddUObject(this, &UVCamOutputProviderBase::OnConnectionReinitialized, WeakWidget);
		}
	});
}

void UVCamOutputProviderBase::StopDetectAndSnapshotWhenConnectionsChange()
{
	check(UMGWidget);
	UUserWidget* Widget = UMGWidget->GetWidget();
	check(Widget);
	
	UE::VCamCore::ForEachWidgetToConsiderForVCam(*Widget, [this](UWidget* Widget)
	{
		if (UVCamWidget* VCamWidget = Cast<UVCamWidget>(Widget))
		{
			VCamWidget->OnPostConnectionsReinitializedDelegate.RemoveAll(this);
		}
	});
}

void UVCamOutputProviderBase::OnConnectionReinitialized(TWeakObjectPtr<UVCamWidget> Widget)
{
	if (Widget.IsValid())
	{
		if (WidgetSnapshot.HasData())
		{
			Modify();
			UE::VCamCore::WidgetSnapshotUtils::Private::RetakeSnapshotForWidgetInHierarchy(WidgetSnapshot, *Widget.Get());
		}
		else if (UMGWidget && ensure(UMGWidget->GetWidget()))
		{
			Modify();
			WidgetSnapshot = UE::VCamCore::WidgetSnapshotUtils::Private::TakeTreeHierarchySnapshot(*UMGWidget->GetWidget());
		}
	}
}
#endif

void UVCamOutputProviderBase::ConditionallySetUpGameplayViewTargets()
{
	UCineCameraComponent* CineCamera = TargetCamera.Get();
	const bool bCannotSetupGameplayViewTargets = !GetWorld()->IsGameWorld() || !CineCamera || !GameplayViewTargetPolicy;
	if (bCannotSetupGameplayViewTargets)
	{
		return;
	}
	
	ConditionallyCleanUpGameplayViewTargets();

	constexpr bool bWillBeActive = true;
	const FDeterminePlayerControllersTargetPolicyParams DeterminePlayersParams{ this, CineCamera, bWillBeActive };
	TArray<APlayerController*> PlayerControllers = GameplayViewTargetPolicy->DeterminePlayerControllers(DeterminePlayersParams);
	PlayerControllers.SetNum(Algo::RemoveIf(PlayerControllers, [Owner = CineCamera->GetOwner()](const APlayerController* PC)
	{
		return PC == nullptr
			// Skip this PC if an earlier output provider already set the view target to our camera 
			|| PC->GetViewTarget() == Owner;
	}));
	
	FUpdateViewTargetPolicyParams UpdateViewTargetParams {{ DeterminePlayersParams }};
	Algo::Transform(PlayerControllers, UpdateViewTargetParams.PlayerControllers, [](APlayerController* PC){ return PC; });
	Algo::Transform(PlayerControllers, PlayersWhoseViewTargetsWereSet, [](APlayerController* PC){ return PC; });
	
	GameplayViewTargetPolicy->UpdateViewTarget(UpdateViewTargetParams);
}

void UVCamOutputProviderBase::ConditionallyCleanUpGameplayViewTargets()
{
	UCineCameraComponent* CineCamera = TargetCamera.Get();
	if (!CineCamera
		|| !IsValid(GameplayViewTargetPolicy)
		// This happens during GC. Not checking this will a check in UpdateViewTarget because BP events are not valid to run when unreachable.
		|| GameplayViewTargetPolicy->IsUnreachable())
	{
		return;
	}

	constexpr bool bWillBeActive = false;
	FUpdateViewTargetPolicyParams Params { { this, CineCamera, bWillBeActive } };
	Algo::TransformIf(PlayersWhoseViewTargetsWereSet, Params.PlayerControllers,
		[](TWeakObjectPtr<APlayerController> PC){ return PC.IsValid(); },
		[](TWeakObjectPtr<APlayerController> PC){ return PC.Get(); }
	);
	PlayersWhoseViewTargetsWereSet.Reset();
	GameplayViewTargetPolicy->UpdateViewTarget(Params);
}

#undef LOCTEXT_NAMESPACE