// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modifiers/ActorModifierCoreExtension.h"
#include "UObject/WeakInterfacePtr.h"
#include "AvaRenderStateUpdateModifierExtension.generated.h"

class AActor;
class UActorComponent;

UINTERFACE(MinimalAPI, NotBlueprintType, meta=(CannotImplementInterfaceInBlueprint))
class UAvaRenderStateUpdateHandler : public UInterface
{
	GENERATED_BODY()
};

/** Implement this interface to handle extension event */
class IAvaRenderStateUpdateHandler
{
	GENERATED_BODY()

public:
	/** Callback when a render state actor in this world changes */
	virtual void OnRenderStateUpdated(AActor* InActor, UActorComponent* InComponent) = 0;

	/** Callback when a tracked actor visibility has changed */
	virtual void OnActorVisibilityChanged(AActor* InActor) = 0;
};

/**
 * This extension tracks specific actors for render state updates,
 * when an update happens it will invoke IAvaRenderStateUpdateExtension function
 */
class FAvaRenderStateUpdateModifierExtension : public FActorModifierCoreExtension
{

public:
	explicit FAvaRenderStateUpdateModifierExtension(IAvaRenderStateUpdateHandler* InExtensionHandler);

	/** Adds an actor to track for visibility */
	void TrackActorVisibility(AActor* InActor);

	/** Removes a tracked actor for visibility */
	void UntrackActorVisibility(AActor* InActor);

	/** Checks if actor is tracked for visibility */
	bool IsActorVisibilityTracked(AActor* InActor) const;

	/** Sets current tracked actors, removes any actors not included */
	void SetTrackedActorsVisibility(const TSet<TWeakObjectPtr<AActor>>& InActors);

	/** Sets current tracked actors with actor and its children, removes any actors not included */
	void SetTrackedActorVisibility(AActor* InActor, bool bInIncludeChildren);

protected:
	//~ Begin FActorModifierCoreExtension
	virtual void OnExtensionEnabled(EActorModifierCoreEnableReason InReason) override;
	virtual void OnExtensionDisabled(EActorModifierCoreDisableReason InReason) override;
	//~ End FActorModifierCoreExtension

private:
	void OnRenderStateDirty(UActorComponent& InComponent);

	void BindDelegate();
	void UnbindDelegate();

	TWeakInterfacePtr<IAvaRenderStateUpdateHandler> ExtensionHandlerWeak;

	TMap<TWeakObjectPtr<AActor>, bool> TrackedActorsVisibility;
};
