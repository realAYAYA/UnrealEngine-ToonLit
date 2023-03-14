// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelEditorDragDropHandler.h"
#include "Editor/EditorEngine.h"
#include "Engine/World.h"
#include "ObjectTools.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/BrushBuilder.h"
#include "GameFramework/Actor.h"
#include "AssetSelection.h"
#include "Materials/MaterialInterface.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Editor.h"
#include "HitProxies.h"

#define LOCTEXT_NAMESPACE "UnrealEd"

ULevelEditorDragDropHandler::ULevelEditorDragDropHandler()
{
}

bool ULevelEditorDragDropHandler::PreviewDropObjectsAtCoordinates(int32 MouseX, int32 MouseY, UWorld* World, FViewport* Viewport, const FAssetData& AssetData)
{
	bCanDrop = false;
	HintText = FText::GetEmpty();

	if ( !AssetData.IsValid() )
	{
		bCanDrop = false;
		return false;
	}

	if (World && bRunAssetFilter)
	{
		ULevel* CurrentLevel = World->GetCurrentLevel();
		UWorld* CurrentLevelOuterWorld = CurrentLevel ? Cast<UWorld>(CurrentLevel->GetOuter()) : nullptr;  
		UWorld* ReferencingWorld = CurrentLevelOuterWorld ? CurrentLevelOuterWorld : World;
		FAssetReferenceFilterContext AssetReferenceFilterContext;
		AssetReferenceFilterContext.ReferencingAssets.Add(FAssetData(ReferencingWorld));
		
		TSharedPtr<IAssetReferenceFilter> AssetReferenceFilter = GEditor->MakeAssetReferenceFilter(AssetReferenceFilterContext);
		if (AssetReferenceFilter.IsValid())
		{
			FText FailureReason;
			if (!AssetReferenceFilter->PassesFilter(AssetData, &FailureReason))
			{
				bCanDrop = false;
				HintText = FailureReason;
				return false;
			}
		}
	}

	return true;
}

bool ULevelEditorDragDropHandler::PreDropObjectsAtCoordinates(int32 MouseX, int32 MouseY, UWorld* World, FViewport* Viewport, const TArray<UObject*>& DroppedObjects, TArray<AActor*>& OutNewActors)
{
	return true;
}

#undef LOCTEXT_NAMESPACE