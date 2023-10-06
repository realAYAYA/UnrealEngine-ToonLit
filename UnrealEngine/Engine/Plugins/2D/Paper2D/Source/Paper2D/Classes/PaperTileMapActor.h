// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "PaperTileMapActor.generated.h"

class UPaperTileMapComponent;

/**
 * An instance of a UPaperTileMap in a level.
 *
 * This actor is created when you drag a tile map asset from the content browser into the level, and
 * it is just a thin wrapper around a UPaperTileMapComponent that actually references the asset.
 */
UCLASS(ComponentWrapperClass)
class PAPER2D_API APaperTileMapActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(Category=TileMapActor, VisibleAnywhere, BlueprintReadOnly, meta=(ExposeFunctionCategories="Sprite,Rendering,Physics,Components|Sprite", AllowPrivateAccess="true"))
	TObjectPtr<class UPaperTileMapComponent> RenderComponent;
public:

	// AActor interface
#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif
	// End of AActor interface

	/** Returns RenderComponent subobject **/
	FORCEINLINE class UPaperTileMapComponent* GetRenderComponent() const { return RenderComponent; }
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#endif
