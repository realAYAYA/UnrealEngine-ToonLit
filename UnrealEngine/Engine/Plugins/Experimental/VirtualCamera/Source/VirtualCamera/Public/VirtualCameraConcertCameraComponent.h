// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "VirtualCameraConcertCameraComponent.generated.h"

class UCineCameraComponent;

/**
 * A class to transfer Camera data in MU session
 */
UCLASS(deprecated, Blueprintable, ClassGroup = Camera, meta = (BlueprintSpawnableComponent))
class VIRTUALCAMERA_API UDEPRECATED_VirtualCameraConcertCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDEPRECATED_VirtualCameraConcertCameraComponent(const FObjectInitializer& ObjectInitializer);

	/**
	 * The tracked name used by the multi user system to send the data.
	 * When not set the component name will be used. That may conflict with another instance that has the same name but within different actors.
	 */
	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category = "VirtualCamera")
	FName TrackingName;

	/** Is that component sending the camera information or receiving it. */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "VirtualCamera")
	bool bHasAuthority;

	/**
	 * When the component has the authority and we are in the editor, we should broadcast the update.
	 * @note The component needs to be owned by an actor that is ticked in the editor.
	 * @see ShouldTickIfViewportsOnly
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VirtualCamera", meta = (EditCondition = "bHasAuthority"))
	bool bSendUpdateInEditor;

	/** Should update the camera component relative location & rotation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VirtualCamera", meta = (EditCondition = "!bHasAuthority"))
	bool bUpdateCameraComponentTransform;

	/** The camera component that will be tracked. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VirtualCamera")
	TObjectPtr<UCineCameraComponent> ComponentToTrack;

public:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
};
