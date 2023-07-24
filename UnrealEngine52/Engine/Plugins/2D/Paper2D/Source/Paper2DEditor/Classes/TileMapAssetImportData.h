// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorFramework/AssetImportData.h"
#include "TileMapAssetImportData.generated.h"

class UPaperTileSet;

USTRUCT()
struct FTileSetImportMapping
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString SourceName;

	UPROPERTY()
	TWeakObjectPtr<class UPaperTileSet> ImportedTileSet;

	UPROPERTY()
	TWeakObjectPtr<class UTexture> ImportedTexture;
};

/**
 * Base class for import data and options used when importing a tile map
 */
UCLASS()
class PAPER2DEDITOR_API UTileMapAssetImportData : public UAssetImportData
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FTileSetImportMapping> TileSetMap;

	static UTileMapAssetImportData* GetImportDataForTileMap(class UPaperTileMap* TileMap);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "UObject/ObjectMacros.h"
#endif
