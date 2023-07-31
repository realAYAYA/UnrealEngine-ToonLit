// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "Templates/SubclassOf.h"

#include "VCamConnectionStructs.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVCamConnection, Log, All);

class UInputAction;
class UInterface;
class UVCamComponent;
class UVCamModifier;

/*
 * This stores information about the target for a given VCam Widget Connection
 * Ideally this is controlled externally via a configurator system but can be set explicitly if required
 */
USTRUCT(BlueprintType)
struct VCAMCORE_API FVCamConnectionTargetSettings
{
	GENERATED_BODY()

	/*
	 * The name of the Modifier to search for in a given VCam Component's Modifier Stack
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ConnectionBinding)
	FName TargetModifierName = NAME_None;

	/*
	 * The name of the Connection Point to search for in a given VCam Modifier
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ConnectionBinding)
	FName TargetConnectionPoint = NAME_None;

	bool operator==(const FVCamConnectionTargetSettings& Other) const
	{
		return TargetModifierName == Other.TargetModifierName
		&& TargetConnectionPoint == Other.TargetConnectionPoint;
	}

	bool HasValidSettings() const
	{
		return !(TargetModifierName.IsNone() || TargetConnectionPoint.IsNone());
	}
};

/*
 * A VCam Connection allows an external object (primarily widgets) to communicate with VCam Modifiers through Connection Points that have known properties.
 * The connection allows you to specify a set of requirements that a connection point on a modifier must implement
 * for it to be considered a valid thing to connect to and then a function "AttemptConnection" that will try to resolve
 * a given VCam Component to a specific Modifier along with an optional associated Input Action
 */
USTRUCT(BlueprintType)
struct VCAMCORE_API FVCamConnection
{
	GENERATED_BODY()

	/*
	 * A list of interfaces that a modifier must implement to be considered a valid connection
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Connection)
	TArray<TSubclassOf<UInterface>> RequiredInterfaces;

	/*
	 * A list of interfaces that a modifier may optionally implement that this connection can use
	 *
	 * These interfaces are not tested for when attempting a connection but are stored so that
	 * a Configurator system can provide extra information a user when they are configuring connections
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Connection)
	TArray<TSubclassOf<UInterface>> OptionalInterfaces;

	/*
	 * Whether this connection requires a target Connection Point to have an associated Input Action to be considered a valid connection
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Connection)
	bool bRequiresInputAction = true;

	/*
	 * If this connection requires an Input Action then this property specifies what Input Action Type the Connection Point must provide to be considered a valid connection
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Connection, meta=(EditCondition="bRequiresInputAction"))
	EInputActionValueType ActionType = EInputActionValueType::Boolean;

	/*
	 * If you are not using an external system to configure the connection then you can enable this to have
	 * explicit control over which Modifier and Connection Point to look for when attempting a connection
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Connection)
	bool bManuallyConfigureConnection = false;

	/*
	 * A struct containing information about which Modifier and Connection Point to look for when attempting a connection
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Connection, meta=(EditCondition="bManuallyConfigureConnection", EditConditionHides))
	FVCamConnectionTargetSettings ConnectionTargetSettings;

	/*
	 * The Input Action that we are currently connected to
	 *
	 * This could be empty even if we have a valid connection
	 */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ConnectedAction = nullptr;

	/*
	 * The VCam Modifier that we are currently connected to
	 *
	 * Guaranteed to be valid if IsConnected() is true
	 */
	UPROPERTY(Transient)
	TObjectPtr<UVCamModifier> ConnectedModifier = nullptr;

	/*
	 * Check whether this connection was successfully connected to a Connection Point
	 */
	bool IsConnected() const;

	/*
	 * Attempts to make a connection to a target Connection Point and returns a bool indicating success
	 */
	bool AttemptConnection(UVCamComponent* VCamComponent);

	/*
	 * Clears any currently connected modifier and action
	 */
	void ResetConnection();
};
