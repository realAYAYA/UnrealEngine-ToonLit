// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorGizmos/EditorTransformGizmoUtil.h"

#include "ContextObjectStore.h"
#include "EditorInteractiveGizmoSubsystem.h"
#include "EditorInteractiveGizmoManager.h"
#include "InteractiveToolsContext.h"
#include "EditorModeManager.h"
#include "EdModeInteractiveToolsContext.h"
#include "EditorGizmos/TransformGizmo.h"
#include "EditorGizmos/EditorTransformGizmoBuilder.h"
#include "EditorGizmos/EditorTransformGizmoDataBinder.h"
#include "EditorViewportClient.h"
#include "EditorModes.h"
#include "Tools/DefaultEdMode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditorTransformGizmoUtil)

bool UE::EditorTransformGizmoUtil::RegisterTransformGizmoContextObject(FEditorModeTools* InModeTools)
{
	const UModeManagerInteractiveToolsContext* ToolsContext = InModeTools->GetInteractiveToolsContext();
	if (ensure(ToolsContext))
	{
		const UEditorTransformGizmoContextObject* Found = ToolsContext->ContextObjectStore->FindContext<UEditorTransformGizmoContextObject>();
		if (Found)
		{
			return true;
		}

		UEditorTransformGizmoContextObject* GizmoContextObject = NewObject<UEditorTransformGizmoContextObject>(ToolsContext->ToolManager);
		if (ensure(GizmoContextObject))
		{
			GizmoContextObject->RegisterGizmosWithManager(ToolsContext->ToolManager);
			GizmoContextObject->Initialize(InModeTools);
			return true;
		}
	}
	return false;
}

bool UE::EditorTransformGizmoUtil::UnregisterTransformGizmoContextObject(FEditorModeTools* InModeTools)
{
	const UModeManagerInteractiveToolsContext* ToolsContext = InModeTools->GetInteractiveToolsContext();
	if (ensure(ToolsContext))
	{
		UEditorTransformGizmoContextObject* Found = ToolsContext->ContextObjectStore->FindContext<UEditorTransformGizmoContextObject>();
		if (Found != nullptr)
		{
			Found->Shutdown();
			Found->UnregisterGizmosWithManager(ToolsContext->ToolManager);
		}
		return true;
	}
	return false;
}

UTransformGizmo* UE::EditorTransformGizmoUtil::CreateTransformGizmo(
	UInteractiveToolManager* InToolManager, const FString& InInstanceIdentifier, void* InOwner)
{
	if (!InToolManager)
	{
		return nullptr;
	}
	
	UEditorInteractiveGizmoManager* GizmoManager = Cast<UEditorInteractiveGizmoManager>(InToolManager->GetPairedGizmoManager());
	if (!GizmoManager)
	{
		return nullptr;
	}

	const UEditorTransformGizmoContextObject* Found = GizmoManager->GetContextObjectStore()->FindContext<UEditorTransformGizmoContextObject>();
	if (!Found)
	{
		return nullptr;
	}

	return Found->CreateTransformGizmo(GizmoManager, InInstanceIdentifier, InOwner);
}

UTransformGizmo* UE::EditorTransformGizmoUtil::GetDefaultTransformGizmo(UInteractiveToolManager* InToolManager)
{
	return CreateTransformGizmo(InToolManager, UEditorInteractiveGizmoManager::TransformInstanceIdentifier(), nullptr);
}

UTransformGizmo* UE::EditorTransformGizmoUtil::FindDefaultTransformGizmo(UInteractiveToolManager* InToolManager)
{
	if (!InToolManager)
	{
		return nullptr;
	}
	
	const UEditorInteractiveGizmoManager* GizmoManager = Cast<UEditorInteractiveGizmoManager>(InToolManager->GetPairedGizmoManager());
	if (!GizmoManager)
	{
		return nullptr;
	}

	UInteractiveGizmo* Found = GizmoManager->FindGizmoByInstanceIdentifier(UEditorInteractiveGizmoManager::TransformInstanceIdentifier());
	return Cast<UTransformGizmo>(Found);
}

void UE::EditorTransformGizmoUtil::RemoveDefaultTransformGizmo(UInteractiveToolManager* InToolManager)
{
	if (!InToolManager)
	{
		return;
	}
	
	UEditorInteractiveGizmoManager* GizmoManager = Cast<UEditorInteractiveGizmoManager>(InToolManager->GetPairedGizmoManager());
	if (!GizmoManager)
	{
		return;
	}

	UInteractiveGizmo* Found = GizmoManager->FindGizmoByInstanceIdentifier(UEditorInteractiveGizmoManager::TransformInstanceIdentifier());
	if (Found)
	{
		GizmoManager->DestroyGizmo(Found);
	}
}

void UEditorTransformGizmoContextObject::RegisterGizmosWithManager(UInteractiveToolManager* InToolManager)
{
	if (ensure(!bGizmosRegistered) == false)
	{
		return;
	}

	InToolManager->GetContextObjectStore()->AddContextObject(this);
	
	UEditorInteractiveGizmoManager* GizmoManager = Cast<UEditorInteractiveGizmoManager>(InToolManager->GetPairedGizmoManager());
	if (ensure(GizmoManager))
	{
		if (UEditorInteractiveGizmoSubsystem* GizmoSubsystem = GEditor->GetEditorSubsystem<UEditorInteractiveGizmoSubsystem>())
		{
			GizmoManager->RegisterGizmoType(UEditorInteractiveGizmoManager::TransformBuilderIdentifier(), GizmoSubsystem->GetTransformGizmoBuilder());
		}
		else
		{
			UEditorTransformGizmoBuilder* GizmoBuilder = NewObject<UEditorTransformGizmoBuilder>(this);
			GizmoManager->RegisterGizmoType(UEditorInteractiveGizmoManager::TransformBuilderIdentifier(), GizmoBuilder);	
		}
	}
	
	bGizmosRegistered = true;
}

void UEditorTransformGizmoContextObject::UnregisterGizmosWithManager(UInteractiveToolManager* InToolManager)
{
	ensure(bGizmosRegistered);
	
	InToolManager->GetContextObjectStore()->RemoveContextObject(this);

	UInteractiveGizmoManager* GizmoManager = InToolManager->GetPairedGizmoManager();
	GizmoManager->DeregisterGizmoType(UEditorInteractiveGizmoManager::TransformBuilderIdentifier());
	
	bGizmosRegistered = false;
}

void UEditorTransformGizmoContextObject::Initialize(FEditorModeTools* InModeTools)
{
	if (ensure(InModeTools && !ModeTools))
	{
		ModeTools = InModeTools;
		InitializeGizmoManagerBinding();

		DataBinder = MakeShared<FEditorTransformGizmoDataBinder>();
		DataBinder->BindToGizmoContextObject(this);
	}
}

void UEditorTransformGizmoContextObject::Shutdown()
{
	if (ensure(DataBinder))
	{
		DataBinder.Reset();
	}
	
	if (ensure(ModeTools))
	{
		UpdateGizmo({});
		RemoveViewportsBinding();
		RemoveGizmoManagerBinding();
		
		ModeTools = nullptr;
	}
}

UTransformGizmo* UEditorTransformGizmoContextObject::CreateTransformGizmo(
	UEditorInteractiveGizmoManager* InGizmoManager, const FString& InInstanceIdentifier, void* InOwner) const
{
	if (!InGizmoManager)
	{
		return nullptr;
	}
	
	UTransformGizmo* NewGizmo = Cast<UTransformGizmo>( InGizmoManager->CreateGizmo(
		UEditorInteractiveGizmoManager::TransformBuilderIdentifier(), InInstanceIdentifier, InOwner) );

	if (ensure(NewGizmo))
	{
		OnGizmoCreated.Broadcast(NewGizmo);
		InGizmoManager->OnGizmosParametersChangedDelegate().AddUObject(NewGizmo, &UTransformGizmo::OnParametersChanged);
		if (InGizmoManager->GetDefaultGizmosParameters())
		{
			NewGizmo->OnParametersChanged(*InGizmoManager->GetDefaultGizmosParameters());
		}
	}
	
	return NewGizmo;
}

FEditorModeTools* UEditorTransformGizmoContextObject::GetModeTools() const
{
	return ModeTools;
}

UEditorTransformGizmoContextObject::FOnGizmoCreated& UEditorTransformGizmoContextObject::OnGizmoCreatedDelegate()
{
	return OnGizmoCreated;
}

void UEditorTransformGizmoContextObject::UpdateGizmo(const TArray<FEditorViewportClient*>& InViewportClients) const
{
	if (!ModeTools)
	{
		return;
	}

	// get viewports pointing at ModeTools
	const TArray<FEditorViewportClient*> ViewportClients = InViewportClients.FilterByPredicate([this](const FEditorViewportClient* ViewportClient)
	{
		return ViewportClient && ViewportClient->GetModeTools() == ModeTools;
	});

	// remove useless gizmo
	if (ViewportClients.IsEmpty())
	{
		const UModeManagerInteractiveToolsContext* ToolsContext = ModeTools->GetInteractiveToolsContext();
		const TObjectPtr<UInteractiveToolManager> ToolManager = ToolsContext->ToolManager;
		UE::EditorTransformGizmoUtil::RemoveDefaultTransformGizmo(ToolManager);
		return;
	}

	// if the focus viewport has been deleted, switch to the first visible one if any
	// FIXME, use the hover one instead ?
	const bool bIsFocusStillAvailable = ViewportClients.Contains(ModeTools->GetFocusedViewportClient());
	if (!bIsFocusStillAvailable)
	{
		const int32 Index = ViewportClients.IndexOfByPredicate([](const FEditorViewportClient* ViewportClient)
		{
			return ViewportClient->IsVisible();
		});
		if (Index != INDEX_NONE)
		{
			ModeTools->ReceivedFocus(ViewportClients[Index], ViewportClients[Index]->Viewport);
		}
	}
	
	const UModeManagerInteractiveToolsContext* ToolsContext = ModeTools->GetInteractiveToolsContext();
	const TObjectPtr<UInteractiveToolManager> ToolManager = ToolsContext->ToolManager;

	// swap default mode
	const bool Swapped = SwapDefaultMode(FBuiltinEditorModes::EM_Default, FAssetEdModes::EM_AssetDefault);
	
	if (UE::EditorTransformGizmoUtil::FindDefaultTransformGizmo(ToolManager))
	{
		return;
	}
				
	// create a new one
	if (UTransformGizmo* TransformGizmo = UE::EditorTransformGizmoUtil::GetDefaultTransformGizmo(ToolManager))
	{
		TransformGizmo->SetVisibility(Swapped ? UEditorInteractiveGizmoManager::UsesNewTRSGizmos() : false);
	}
}

void UEditorTransformGizmoContextObject::InitializeGizmoManagerBinding()
{
	auto OnGizmoVariableChanged = [this](const bool bUseNewTRSGizmos)
	{
		if (!bUseNewTRSGizmos)
		{
			// swap back default mode
			(void)SwapDefaultMode(FAssetEdModes::EM_AssetDefault, FBuiltinEditorModes::EM_Default);
			
			// remove viewports' binding + gizmos as they are useless
			const UModeManagerInteractiveToolsContext* ToolsContext = ModeTools->GetInteractiveToolsContext();
			const TObjectPtr<UInteractiveToolManager> ToolManager = ToolsContext->ToolManager;
			UE::EditorTransformGizmoUtil::RemoveDefaultTransformGizmo(ToolManager);
			
			RemoveViewportsBinding();
			
			return;
		}

		InitializeViewportsBinding();
	};

	// bind UseNewGizmosChangedHandled if needed
	if (!UseNewGizmosChangedHandled.IsValid())
	{
		UseNewGizmosChangedHandled = UEditorInteractiveGizmoManager::OnUsesNewTRSGizmosChangedDelegate().AddLambda(OnGizmoVariableChanged);
	}
	
	// initialize default viewport list change binding
	OnGizmoVariableChanged(UEditorInteractiveGizmoManager::UsesNewTRSGizmos());
}

void UEditorTransformGizmoContextObject::InitializeViewportsBinding()
{
	if (GEditor)
	{
		// check added/removed viewports to ensure that ModeTools' FocusedViewportClient is still safe to use
		auto OnViewportClientsChanged = [this]()
		{
			const TArray<FEditorViewportClient*> ViewportClients =
				GEditor->GetAllViewportClients().FilterByPredicate([this](const FEditorViewportClient* ViewportClient)
				{
					return ViewportClient && ViewportClient->GetModeTools() == ModeTools;
				});

			UpdateGizmo(ViewportClients);
		};

		// bind ViewportClientsChangedHandle if needed
        if (!ViewportClientsChangedHandle.IsValid())
        {
        	ViewportClientsChangedHandle = GEditor->OnViewportClientListChanged().AddLambda(OnViewportClientsChanged);
        }
    
		// initialize
        OnViewportClientsChanged();
	}
}

void UEditorTransformGizmoContextObject::RemoveGizmoManagerBinding()
{
	if (UseNewGizmosChangedHandled.IsValid())
	{
		UEditorInteractiveGizmoManager::OnUsesNewTRSGizmosChangedDelegate().Remove(UseNewGizmosChangedHandled);
		UseNewGizmosChangedHandled.Reset();
	}
}

void UEditorTransformGizmoContextObject::RemoveViewportsBinding()
{
	if (ViewportClientsChangedHandle.IsValid())
	{
		if (ensure(GEditor))
		{
			GEditor->OnViewportClientListChanged().Remove(ViewportClientsChangedHandle);
		}
		ViewportClientsChangedHandle.Reset();
	}
}

bool UEditorTransformGizmoContextObject::SwapDefaultMode(const FEditorModeID InCurrentDefaultMode, const FEditorModeID InNewDefaultMode) const
{
	// swap only if we're not dealing with the level editor + InCurrentDefaultMode is one of the default modes
	if (!ModeTools->IsDefaultMode(InCurrentDefaultMode))
	{
		return false;
	}
	
	const bool bIsLevelMode = ModeTools == &GLevelEditorModeTools();
	if (bIsLevelMode)
	{
		return false;
	}
	
	// replace default mode
	ModeTools->RemoveDefaultMode(InCurrentDefaultMode);
	ModeTools->AddDefaultMode(InNewDefaultMode);
	
	if (ModeTools->IsModeActive(InCurrentDefaultMode))
	{
		// activate it if the previous one was 
		ModeTools->DeactivateMode(InCurrentDefaultMode);
		ModeTools->ActivateMode(InNewDefaultMode);
	}

	return true;
}
