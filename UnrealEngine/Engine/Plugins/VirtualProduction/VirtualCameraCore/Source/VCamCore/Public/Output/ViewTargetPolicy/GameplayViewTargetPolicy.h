// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "GameplayViewTargetPolicy.generated.h"

class APlayerController;
class UCineCameraComponent;
class UVCamOutputProviderBase;

USTRUCT(BlueprintType)
struct VCAMCORE_API FDeterminePlayerControllersTargetPolicyParams
{
	GENERATED_BODY()

	/** The output provider in question */
	UPROPERTY(BlueprintReadWrite, Category = "VirtualCamera")
	TObjectPtr<UVCamOutputProviderBase> OutputProvider;

	/** The camera the output provider is associated with. */
	UPROPERTY(BlueprintReadWrite, Category = "VirtualCamera")
	TObjectPtr<UCineCameraComponent> CameraToAffect;
	
	/** Whether the output provider will be made active or inactive. */
	UPROPERTY(BlueprintReadWrite, Category = "VirtualCamera")
	bool bNewIsActive {};
};

USTRUCT(BlueprintType)
struct VCAMCORE_API FUpdateViewTargetPolicyParams : public FDeterminePlayerControllersTargetPolicyParams
{
	GENERATED_BODY()

	/** The player controllers whose view targets should be set to CameraToAffect. */
	UPROPERTY(BlueprintReadWrite, Category = "VirtualCamera")
	TArray<TObjectPtr<APlayerController>> PlayerControllers;
};

/**
 * A gameplay view target policy determines which player controller should change its view target when gameplay is started,
 * e.g. PIE or in shipped games.
 *
 * Some output providers, such as pixel streaming, require the view target to be set to the cine camera
 */
UCLASS(Abstract, Blueprintable, EditInlineNew)
class VCAMCORE_API UGameplayViewTargetPolicy : public UObject
{
	GENERATED_BODY()
public:

	/**
	 * Called when the policy should be applied: when the output provider's activation changes in a game world.
	 * @return The player controllers for which the view target should be set to the output provider's camera. Typically contains 1 element but you can return more if you wish.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera")
	TArray<APlayerController*> DeterminePlayerControllers(const FDeterminePlayerControllersTargetPolicyParams& Params);

	/**
	 * Called to just after the view target has been updated for the player controller that was previously returned by DeterminePlayerController.
	 *
	 * The default implementation sets the view target without a blend. You can override this to do a blend or perform additional actions.
	 * Note this is also called when the the owning output provider is deactivated (so you can possibly reset the view target to be the old player controller's pawn).
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "VirtualCamera")
	void UpdateViewTarget(const FUpdateViewTargetPolicyParams& Params);
};