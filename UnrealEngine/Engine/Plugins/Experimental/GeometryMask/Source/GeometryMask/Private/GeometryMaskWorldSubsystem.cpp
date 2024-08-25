// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskWorldSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GeometryMaskSVE.h"
#include "SceneViewExtension.h"

void UGeometryMaskWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();

	GeometryMaskSceneViewExtension = FSceneViewExtensions::NewExtension<FGeometryMaskSceneViewExtension>(World);
}

void UGeometryMaskWorldSubsystem::Deinitialize()
{
	Super::Deinitialize();

	for (const TPair<FName, TObjectPtr<UGeometryMaskCanvas>>& NamedCanvas : NamedCanvases)
	{
		if (IsValid(NamedCanvas.Value))
		{
			NamedCanvas.Value->Free();	
		}
	}

	NamedCanvases.Empty();

	if (UGeometryMaskSubsystem* EngineSubsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>())
	{
		EngineSubsystem->OnWorldDestroyed(GetWorld());
	}
}

UGeometryMaskCanvas* UGeometryMaskWorldSubsystem::GetNamedCanvas(FName InName)
{
	UGeometryMaskSubsystem* EngineSubsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>();
	if (!ensureAlwaysMsgf(EngineSubsystem, TEXT("UGeometryMaskSubsystem not resolved.")))
	{
		return nullptr;
	}
	
	if (InName.IsNone())
	{
		return EngineSubsystem->GetDefaultCanvas();
	}

	const FName ObjectName = MakeUniqueObjectName(this, UGeometryMaskCanvas::StaticClass(), FName(FString::Printf(TEXT("GeometryMaskCanvas_%s_"), *InName.ToString())));
	if (TObjectPtr<UGeometryMaskCanvas>* FoundCanvas = NamedCanvases.Find(InName))
	{
		return *FoundCanvas;
	}

	const TObjectPtr<UGeometryMaskCanvas>& NewCanvas = NamedCanvases.Emplace(InName, NewObject<UGeometryMaskCanvas>(this, ObjectName));
	NewCanvas->Initialize(GetWorld(), InName);
	EngineSubsystem->AssignResourceToCanvas(NewCanvas);

	NewCanvas->OnActivated().BindUObject(this, &UGeometryMaskWorldSubsystem::OnCanvasActivated, NewCanvas.Get());
	NewCanvas->OnDeactivated().BindUObject(this, &UGeometryMaskWorldSubsystem::OnCanvasDeactivated, NewCanvas.Get());
	
	OnGeometryMaskCanvasCreatedDelegate.Broadcast(NewCanvas);

	return NewCanvas;
}

TArray<FName> UGeometryMaskWorldSubsystem::GetCanvasNames()
{
	TArray<FName> CanvasNames;
	NamedCanvases.GenerateKeyArray(CanvasNames);
	return CanvasNames;
}

int32 UGeometryMaskWorldSubsystem::RemoveWithoutWriters()
{
	int32 NumRemoved = 0;
	
	TMap<FName, TObjectPtr<UGeometryMaskCanvas>> UsedCanvases;
	UsedCanvases.Reserve(NamedCanvases.Num());

	for (const TPair<FName, TObjectPtr<UGeometryMaskCanvas>>& NamedCanvas : NamedCanvases)
	{
		if (!IsValid(NamedCanvas.Value))
		{
			continue;
		}
		
		if (!NamedCanvas.Value->GetWriters().IsEmpty())
		{
			UsedCanvases.Emplace(NamedCanvas.Key, NamedCanvas.Value);
		}
		else
		{
			OnGeometryMaskCanvasDestroyed().Broadcast(NamedCanvas.Value->GetCanvasId());
			NamedCanvas.Value->FreeResource();
			++NumRemoved;
		}
	}
	
	NamedCanvases = UsedCanvases;

	return NumRemoved;
}

void UGeometryMaskWorldSubsystem::OnCanvasActivated(UGeometryMaskCanvas* InCanvas)
{
	check(InCanvas);
	
	if (InCanvas->IsDefaultCanvas())
	{
		return;
	}

	// Already has a resource
	if (InCanvas->GetResource())
	{
		return;
	}

	UGeometryMaskSubsystem* EngineSubsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>();
	if (!ensureAlwaysMsgf(EngineSubsystem, TEXT("UGeometryMaskSubsystem not resolved.")))
	{
		return;
	}

	// Provide a new resource for the canvas to write to
	EngineSubsystem->AssignResourceToCanvas(InCanvas);
}

void UGeometryMaskWorldSubsystem::OnCanvasDeactivated(UGeometryMaskCanvas* InCanvas)
{
	if (!InCanvas || !IsValid(InCanvas) || InCanvas->IsDefaultCanvas())
	{
		return;
	}

	if (const UGeometryMaskCanvasResource* CanvasResource = InCanvas->GetResource())
	{
		// Resource assigned, so free it up
		OnGeometryMaskCanvasDestroyed().Broadcast(InCanvas->GetCanvasId());
		InCanvas->FreeResource();
	}
}
