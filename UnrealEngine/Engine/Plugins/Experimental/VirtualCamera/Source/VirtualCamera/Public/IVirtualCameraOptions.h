// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "IVirtualCameraOptions.generated.h"

UINTERFACE(Blueprintable)
class VIRTUALCAMERA_API UVirtualCameraOptions : public UInterface
{
	GENERATED_BODY()
};

class VIRTUALCAMERA_API IVirtualCameraOptions
{
	GENERATED_BODY()
	
public:

	/**
	 * Sets unit of distance.
	 * @param DesiredUnits - The new unit to use for distance measures like focus distance
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Options")
	void SetDesiredDistanceUnits(const EUnit DesiredUnits);

	/**
	 * Returns previously set unit of distance.
	 * @return DesiredUnits - The unit that is used for distance measures like focus distance
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Options")
	EUnit GetDesiredDistanceUnits();

	/**
	 * Checks whether or not focus visualization can activate
	 * @return the current state of touch event visualization
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Options")
	bool IsFocusVisualizationAllowed();
};
