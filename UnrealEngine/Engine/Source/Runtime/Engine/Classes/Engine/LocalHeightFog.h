// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "LocalHeightFog.generated.h"

class ULocalHeightFogComponent;

/**
 *	Actor used to position a local fog volume in the scene.
 *	@see https://docs.unrealengine.com/???
 */
UCLASS(showcategories = (Movement, Rendering, Transformation, DataLayers, "Input|MouseInput", "Input|TouchInput"), ClassGroup = Fog, hidecategories = (Info, Object, Input), MinimalAPI)
class ALocalHeightFog : public AInfo
{
	GENERATED_UCLASS_BODY()

private:
#if WITH_EDITOR
	virtual bool IsDataLayerTypeSupported(TSubclassOf<UDataLayerInstance> DataLayerType) const override { return true; }
#endif

	/** Object used to visualize the local fog volume */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Fog, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULocalHeightFogComponent> LocalHeightFogVolume;

public:

	/** Returns LocalHeightFogVolume subobject **/
	ULocalHeightFogComponent* GetComponent() const { return LocalHeightFogVolume; }
};

