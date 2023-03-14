// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamOutputProviderBase.h"

#include "VCamModifierInterface.h"
#include "VCamComponent.h"
#include "UObject/UObjectBaseUtility.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "UI/VCamWidget.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

DEFINE_LOG_CATEGORY(LogVCamOutputProvider);

UVCamOutputProviderBase::UVCamOutputProviderBase()
	: bIsActive(false)
	, bInitialized(false)
{

}

UVCamOutputProviderBase::~UVCamOutputProviderBase()
{
	// Deinitialize can't be done here since the destruction order isn't guaranteed
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
			if (IsOuterComponentEnabled())
			{
				Activate();
			}
		}
	}
}

void UVCamOutputProviderBase::Deinitialize()
{
	if (bInitialized)
	{
		Deactivate();
		bInitialized = false;
	}
	
}

void UVCamOutputProviderBase::Activate()
{
	CreateUMG();
	DisplayUMG();
}

void UVCamOutputProviderBase::Deactivate()
{
	DestroyUMG();
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

	if (IsOuterComponentEnabled())
	{
		if (bIsActive)
		{
			Activate();
		}
		else
		{
			Deactivate();
		}
	}
}

bool UVCamOutputProviderBase::IsOuterComponentEnabled() const
{
	if (UVCamComponent* OuterComponent = GetTypedOuter<UVCamComponent>())
	{
		return OuterComponent->IsEnabled();
	}

	return false;
}


void UVCamOutputProviderBase::SetTargetCamera(const UCineCameraComponent* InTargetCamera)
{
	TargetCamera = InTargetCamera;

	NotifyWidgetOfComponentChange();
}

void UVCamOutputProviderBase::SetUMGClass(const TSubclassOf<UUserWidget> InUMGClass)
{
	UMGClass = InUMGClass;
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

	UMGWidget = NewObject<UVPFullScreenUserWidget>(this, UVPFullScreenUserWidget::StaticClass());
	UMGWidget->SetDisplayTypes(DisplayType, DisplayType, DisplayType);
	UMGWidget->PostProcessDisplayType.bReceiveHardwareInput = true;

#if WITH_EDITOR
	UMGWidget->SetAllTargetViewports(GetTargetLevelViewport());
#endif

	UMGWidget->WidgetClass = UMGClass;
	UE_LOG(LogVCamOutputProvider, Log, TEXT("CreateUMG widget named %s from class %s"), *UMGWidget->GetName(), *UMGWidget->WidgetClass->GetName());
}

void UVCamOutputProviderBase::DisplayUMG()
{
	if (UMGWidget)
	{
		UWorld* ActorWorld = nullptr;
		int32 WorldType = -1;

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World())
			{
				// Prioritize PIE and Game modes if active
				if ((Context.WorldType == EWorldType::PIE) || (Context.WorldType == EWorldType::Game))
				{
					ActorWorld = Context.World();
					WorldType = (int32)Context.WorldType;
					break;
				}
				else if (Context.WorldType == EWorldType::Editor)
				{
					// Only grab the Editor world if PIE and Game aren't available
					ActorWorld = Context.World();
					WorldType = (int32)Context.WorldType;
				}
			}
		}

		if (ActorWorld)
		{
			UMGWidget->Display(ActorWorld);
			UE_LOG(LogVCamOutputProvider, Log, TEXT("DisplayUMG widget displayed in WorldType %d"), WorldType);
		}

		NotifyWidgetOfComponentChange();
	}
}

void UVCamOutputProviderBase::DestroyUMG()
{
	if (UMGWidget)
	{
		if (UMGWidget->IsDisplayed())
		{
			UMGWidget->Hide();
			UE_LOG(LogVCamOutputProvider, Log, TEXT("DestroyUMG widget %s hidden"), *UMGWidget->GetName());
		}
		UE_LOG(LogVCamOutputProvider, Log, TEXT("DestroyUMG widget %s destroyed"), *UMGWidget->GetName());

#if WITH_EDITOR
		UMGWidget->ResetAllTargetViewports();
#endif

		UMGWidget->ConditionalBeginDestroy();
		UMGWidget = nullptr;
	}
}

void UVCamOutputProviderBase::NotifyWidgetOfComponentChange() const
{
	if (UMGWidget && UMGWidget->IsDisplayed())
	{
		UUserWidget* DisplayedWidget = UMGWidget->GetWidget();
		if (IsValid(DisplayedWidget))
		{
			if (UVCamComponent* OwningComponent = GetTypedOuter<UVCamComponent>())
			{
				UVCamComponent* VCamComponent = bIsActive ? OwningComponent : nullptr;

				if (DisplayedWidget->Implements<UVCamModifierInterface>())
				{
					IVCamModifierInterface::Execute_OnVCamComponentChanged(DisplayedWidget, VCamComponent);
				}

				// Find all VCam Widgets inside the displayed widget and Initialize them with the owning VCam Component
				if (IsValid(DisplayedWidget->WidgetTree))
				{
					DisplayedWidget->WidgetTree->ForEachWidget([VCamComponent](UWidget* Widget)
					{
						if (UVCamWidget* VCamWidget = Cast<UVCamWidget>(Widget))
						{
							VCamWidget->InitializeConnections(VCamComponent);
						}
					});
				}
			}
		}

	}
}

UVCamOutputProviderBase* UVCamOutputProviderBase::GetOtherOutputProviderByIndex(int32 Index) const
{
	if (Index > INDEX_NONE)
	{
		if (UVCamComponent* OuterComponent = GetTypedOuter<UVCamComponent>())
		{
			if (UVCamOutputProviderBase* Provider = OuterComponent->GetOutputProviderByIndex(Index))
			{
				return Provider;
			}
			else
			{
				UE_LOG(LogVCamOutputProvider, Warning, TEXT("GetOtherOutputProviderByIndex - specified index is out of range"));
			}
		}
	}

	return nullptr;
}


TSharedPtr<FSceneViewport> UVCamOutputProviderBase::GetTargetSceneViewport() const
{
	TSharedPtr<FSceneViewport> SceneViewport;

	if (UVCamComponent* OuterComponent = GetTypedOuter<UVCamComponent>())
	{
		SceneViewport = OuterComponent->GetTargetSceneViewport();
	}

	return SceneViewport;
}

TWeakPtr<SWindow> UVCamOutputProviderBase::GetTargetInputWindow() const
{
	TWeakPtr<SWindow> InputWindow;

	if (UVCamComponent* OuterComponent = GetTypedOuter<UVCamComponent>())
	{
		InputWindow = OuterComponent->GetTargetInputWindow();
	}

	return InputWindow;
}

#if WITH_EDITOR
FLevelEditorViewportClient* UVCamOutputProviderBase::GetTargetLevelViewportClient() const
{
	FLevelEditorViewportClient* ViewportClient = nullptr;

	if (UVCamComponent* OuterComponent = GetTypedOuter<UVCamComponent>())
	{
		ViewportClient = OuterComponent->GetTargetLevelViewportClient();
	}

	return ViewportClient;
}

TSharedPtr<SLevelViewport> UVCamOutputProviderBase::GetTargetLevelViewport() const
{
	TSharedPtr<SLevelViewport> LevelViewport;

	if (UVCamComponent* OuterComponent = GetTypedOuter<UVCamComponent>())
	{
		LevelViewport = OuterComponent->GetTargetLevelViewport();
	}

	return LevelViewport;
}

void UVCamOutputProviderBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FProperty* Property = PropertyChangedEvent.MemberProperty;
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
		&& Property
		&& PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		static FName NAME_IsActive = GET_MEMBER_NAME_CHECKED(UVCamOutputProviderBase, bIsActive);
		static FName NAME_UMGClass = GET_MEMBER_NAME_CHECKED(UVCamOutputProviderBase, UMGClass);

		if (Property->GetFName() == NAME_IsActive)
		{
			SetActive(bIsActive);
		}
		else if (Property->GetFName() == NAME_UMGClass)
		{
			if (bIsActive)
			{
				SetActive(false);
				SetActive(true);
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
