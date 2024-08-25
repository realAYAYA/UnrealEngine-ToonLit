// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modifiers/ActorModifierCoreExtension.h"
#include "UObject/WeakInterfacePtr.h"
#include "AvaTransformUpdateModifierExtension.generated.h"

UINTERFACE(MinimalAPI, NotBlueprintType, meta=(CannotImplementInterfaceInBlueprint))
class UAvaTransformUpdateHandler : public UInterface
{
	GENERATED_BODY()
};

/** Implement this interface to handle extension event */
class IAvaTransformUpdateHandler
{
	GENERATED_BODY()

public:
	/** Callback when a tracked actor transform changes */
	virtual void OnTransformUpdated(AActor* InActor, bool bInParentMoved) = 0;
};

/**
 * This extension tracks specific actors for transform updates,
 * when an update happens it will invoke the IAvaTransformUpdateExtension function
 */
class FAvaTransformUpdateModifierExtension : public FActorModifierCoreExtension
{

public:
	explicit FAvaTransformUpdateModifierExtension(IAvaTransformUpdateHandler* InExtensionHandler);

	void TrackActor(AActor* InActor, bool bInReset);
	void UntrackActor(AActor* InActor);

	void TrackActors(const TSet<TWeakObjectPtr<AActor>>& InActors, bool bInReset);
	void UntrackActors(const TSet<TWeakObjectPtr<AActor>>& InActors);

protected:
	//~ Begin FActorModifierCoreExtension
	virtual void OnExtensionEnabled(EActorModifierCoreEnableReason InReason) override;
	virtual void OnExtensionDisabled(EActorModifierCoreDisableReason InReason) override;
	//~ End FActorModifierCoreExtension

private:
	void OnTransformUpdated(USceneComponent* InComponent, EUpdateTransformFlags InFlags, ETeleportType InType);

	void BindDelegate(const AActor* InActor);
	void UnbindDelegate(const AActor* InActor);

	TWeakInterfacePtr<IAvaTransformUpdateHandler> ExtensionHandlerWeak;

	TSet<TWeakObjectPtr<AActor>> TrackedActors;
};