// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "PaperSpriteActor.generated.h"

class UPaperSpriteComponent;

/**
 * An instance of a UPaperSprite in a level.
 *
 * This actor is created when you drag a sprite asset from the content browser into the level, and
 * it is just a thin wrapper around a UPaperSpriteComponent that actually references the asset.
 */
UCLASS(ComponentWrapperClass, meta = (ChildCanTick))
class PAPER2D_API APaperSpriteActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(Category=Sprite, VisibleAnywhere, BlueprintReadOnly, meta=(ExposeFunctionCategories="Sprite,Rendering,Physics,Components|Sprite", AllowPrivateAccess="true"))
	TObjectPtr<class UPaperSpriteComponent> RenderComponent;
public:

	// AActor interface
#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif
	// End of AActor interface

	/** Returns RenderComponent subobject **/
	FORCEINLINE class UPaperSpriteComponent* GetRenderComponent() const { return RenderComponent; }
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#endif
