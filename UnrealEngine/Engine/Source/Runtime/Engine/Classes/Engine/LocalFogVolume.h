// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "LocalFogVolume.generated.h"

class ULocalFogVolumeComponent;

/**
 *	Actor used to position a local fog volume in the scene.
 *	@see https://docs.unrealengine.com/???
 */
UCLASS(showcategories = (Movement, Rendering, Transformation, DataLayers, "Input|MouseInput", "Input|TouchInput"), ClassGroup = Fog, hidecategories = (Info, Object, Input), MinimalAPI)
class ALocalFogVolume : public AInfo
{
	GENERATED_UCLASS_BODY()

private:
#if WITH_EDITOR
	virtual bool ActorTypeSupportsDataLayer() const override { return true; }
#endif

	/** Object used to visualize the local fog volume */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Fog, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULocalFogVolumeComponent> LocalFogVolumeVolume;

public:

	/** Returns LocalFogVolumeVolume subobject **/
	ULocalFogVolumeComponent* GetComponent() const { return LocalFogVolumeVolume; }
};

