// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperSprite.h"
#include "SpriteDrawCall.generated.h"

//
USTRUCT()
struct PAPER2D_API FSpriteDrawCallRecord
{
public:
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector Destination;

	UPROPERTY()
	TObjectPtr<UTexture> BaseTexture;

	FAdditionalSpriteTextureArray AdditionalTextures;

	UPROPERTY()
	FColor Color;

	//@TODO: The rest of this struct doesn't need to be properties either, but has to be due to serialization for now
	// Render triangle list (stored as loose vertices)
	TArray< FVector4, TInlineAllocator<6> > RenderVerts;

	void BuildFromSprite(const class UPaperSprite* Sprite);

	FSpriteDrawCallRecord()
		: Destination(ForceInitToZero)
		, BaseTexture(nullptr)
		, Color(FColor::White)
	{
	}

	bool IsValid() const
	{
		return (RenderVerts.Num() > 0) && (BaseTexture != nullptr) && (BaseTexture->GetResource() != nullptr);
	}
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#endif
