// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

class USceneComponent;
class FJsonObject;
class FString;
class AActor;

namespace DatasmithImportFactoryHelper
{
	TSharedPtr<FJsonObject> LoadJsonFile(const FString& JsonFilename);

	void ComputeBounds( USceneComponent* ActorComponent, FBox& Bounds );

	void SetupSceneViewport(AActor* SceneActor, TArray<FAssetData>& AssetDataList);
}
