// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OpenColorIODisplayExtension.h"

#include "OpenColorIODisplayExtensionWrapper.generated.h"

/** 
 * This Blueprintable object can hold an OCIO Scene View Extension. 
 * You can change its OCIO config, and specify the context in which you want it to be active on.
 */
UCLASS(Blueprintable)
class OPENCOLORIO_API UOpenColorIODisplayExtensionWrapper : public UObject
{
	GENERATED_BODY()

public:

	// Sets a new OCIO configuration.
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set OpenColorIO Configuration"), Category = "OpenColorIO")
	void SetOpenColorIOConfiguration(FOpenColorIODisplayConfiguration InDisplayConfiguration);

	// Sets a single activation function. Will remove any others.
	UFUNCTION(BlueprintCallable, Category = "OpenColorIO")
	void SetSceneExtensionIsActiveFunction(const FSceneViewExtensionIsActiveFunctor& IsActiveFunction);

	// Sets an array of activation functions. Will remove any others.
	UFUNCTION(BlueprintCallable, Category = "OpenColorIO")
	void SetSceneExtensionIsActiveFunctions(const TArray<FSceneViewExtensionIsActiveFunctor>& IsActiveFunctions);

	// Removes the extension.
	UFUNCTION(BlueprintCallable, Category = "OpenColorIO")
	void RemoveSceneExtension();

	// Creates an instance of this object, configured with the given arguments (OCIO and activation function).
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Create OpenColorIO Display Extension"), Category = "OpenColorIO")
	static UOpenColorIODisplayExtensionWrapper* CreateOpenColorIODisplayExtension(
		FOpenColorIODisplayConfiguration InDisplayConfiguration, 
		const FSceneViewExtensionIsActiveFunctor& IsActiveFunction);

	// Creates an instance of this object, configured for use in game with the given OCIO configuration.
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Create In-Game OpenColorIO Display Extension"), Category = "OpenColorIO")
	static UOpenColorIODisplayExtensionWrapper* CreateInGameOpenColorIODisplayExtension(
		FOpenColorIODisplayConfiguration InDisplayConfiguration);
private:

	// Creates OCIO Extension if it doesn't exist. 
	void CreateDisplayExtensionIfNotExists();

private:

	// Holds the OCIO Extension.
	TSharedPtr<FOpenColorIODisplayExtension, ESPMode::ThreadSafe> DisplayExtension;
};


