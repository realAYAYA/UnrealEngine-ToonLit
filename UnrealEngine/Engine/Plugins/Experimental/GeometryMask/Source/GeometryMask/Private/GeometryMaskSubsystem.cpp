// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GeometryMaskCanvas.h"
#include "GeometryMaskCanvasResource.h"
#include "GeometryMaskModule.h"
#include "GeometryMaskWorldSubsystem.h"
#include "SceneView.h"
#include "UnrealClient.h"

UGeometryMaskCanvas* UGeometryMaskSubsystem::GetDefaultCanvas()
{
	if (!DefaultCanvas)
	{
		const FName DefaultCanvasObjectName = FName(TEXT("GeometryMaskCanvas_Default"));
		DefaultCanvas = NewObject<UGeometryMaskCanvas>(this, DefaultCanvasObjectName);
		DefaultCanvas->Initialize(nullptr, FGeometryMaskCanvasId::DefaultCanvasName);
		AssignResourceToCanvas(DefaultCanvas);
	}
	
	return DefaultCanvas;
}

int32 UGeometryMaskSubsystem::GetNumCanvasResources() const
{
	return CanvasResources.Num();
}

const TSet<TObjectPtr<UGeometryMaskCanvasResource>>& UGeometryMaskSubsystem::GetCanvasResources() const
{
	return CanvasResources;
}

void UGeometryMaskSubsystem::Update(
	UWorld* InWorld,
	FSceneViewFamily& InViewFamily)
{
	if (!bDoUpdates)
	{
		return;
	}

	UE_LOG(LogGeometryMask, VeryVerbose, TEXT("UGeometryMaskSubsystem::Update World: %s, Num. Views: %u"), *InWorld->GetName(), InViewFamily.Views.Num());
	
	if (UGeometryMaskWorldSubsystem* Subsystem = InWorld->GetSubsystem<UGeometryMaskWorldSubsystem>())
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UGeometryMaskSubsystem::Update"), STAT_GeometryMask_UpdateAll, STATGROUP_GeometryMask);

		int32 ViewIndex = 0;
		for (const FSceneView*& View : InViewFamily.Views)
		{
			FSceneView* MutableSceneView = const_cast<FSceneView*>(View);

			for (const TPair<FName, TObjectPtr<UGeometryMaskCanvas>>& NamedCanvas : Subsystem->NamedCanvases)
			{
				NamedCanvas.Value->Update(InWorld, *MutableSceneView);		
			}

			// Updates the texture resource
			for (const TObjectPtr<UGeometryMaskCanvasResource>& Resource : CanvasResources)
			{
				Resource->Update(InWorld, *MutableSceneView, ViewIndex);
			}

			++ViewIndex;
		}
	}
}

void UGeometryMaskSubsystem::ToggleUpdate(const TOptional<bool>& bInShouldUpdate)
{
	// New value should be user provided, or the inverse of the existing value (toggle)
	bool bShouldUpdate = bInShouldUpdate.Get(!bDoUpdates);
	if (bDoUpdates != bShouldUpdate)
	{
		bDoUpdates = bShouldUpdate;
	}
}

void UGeometryMaskSubsystem::AssignResourceToCanvas(UGeometryMaskCanvas* InCanvas)
{
	UGeometryMaskCanvasResource* AvailableResource = nullptr;
	EGeometryMaskColorChannel AvailableChannel = EGeometryMaskColorChannel::None;
	for (UGeometryMaskCanvasResource* CanvasResource : CanvasResources)
	{
		if (AvailableChannel = CanvasResource->GetNextAvailableColorChannel();
			AvailableChannel != EGeometryMaskColorChannel::None)
		{
			AvailableResource = CanvasResource;
		}
	}

	// Nothing available, create new resource
	if (AvailableChannel == EGeometryMaskColorChannel::None)
	{
		AvailableResource = NewObject<UGeometryMaskCanvasResource>(this);
		CanvasResources.Emplace(AvailableResource);
		AvailableChannel = AvailableResource->GetNextAvailableColorChannel();
		OnGeometryMaskResourceCreatedDelegate.Broadcast(AvailableResource);
	}

	AvailableResource->Checkout(AvailableChannel, InCanvas->GetCanvasId());
	InCanvas->AssignResource(AvailableResource, AvailableChannel);
}

void UGeometryMaskSubsystem::CompactResources()
{
	if (CanvasResources.Num() <= 1)
	{
		return;
	}

	TSet<TObjectPtr<UGeometryMaskCanvasResource>> UsedResources;
	UsedResources.Reserve(CanvasResources.Num());
	
	for (UGeometryMaskCanvasResource* CanvasResource : CanvasResources)
	{
		CanvasResource->Compact();
		if (CanvasResource->IsAnyChannelUsed())
		{
			UsedResources.Emplace(CanvasResource);
		}
		else
		{
			OnGeometryMaskResourceDestroyedDelegate.Broadcast(CanvasResource);
		}
	}

	if (UsedResources.Num() != CanvasResources.Num())
	{
		CanvasResources = UsedResources;
	}
}

void UGeometryMaskSubsystem::OnWorldDestroyed(UWorld* InWorld)
{
	CompactResources();
	
	TSet<TObjectPtr<UGeometryMaskCanvasResource>> UnusedResources;
	UnusedResources.Reserve(CanvasResources.Num());
	
	for (UGeometryMaskCanvasResource* CanvasResource : CanvasResources)
	{
		if (!CanvasResource->IsAnyChannelUsed())
		{
			OnGeometryMaskResourceDestroyedDelegate.Broadcast(CanvasResource);
			UnusedResources.Emplace(CanvasResource);
		}
	}

	if (UnusedResources.IsEmpty())
	{
		return;
	}

	CanvasResources = CanvasResources.Difference(UnusedResources);
}
