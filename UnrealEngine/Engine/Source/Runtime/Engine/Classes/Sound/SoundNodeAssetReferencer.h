// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Sound/SoundNode.h"
#include "SoundNodeAssetReferencer.generated.h"

/** 
 * Sound node that contains a reference to the raw wave file to be played
 */
UCLASS(abstract)
class ENGINE_API USoundNodeAssetReferencer : public USoundNode
{
	GENERATED_BODY()

public:
	virtual void LoadAsset(bool bAddToRoot = false) PURE_VIRTUAL(USoundNodeAssetReferencer::LoadAsset,);
	virtual void ClearAssetReferences() PURE_VIRTUAL(USoundNodeAssetReferencer::ClearAssetReferences, );

	bool ShouldHardReferenceAsset(const class ITargetPlatform*) const;

#if WITH_EDITOR
	virtual void PostEditImport() override;
#endif
};

