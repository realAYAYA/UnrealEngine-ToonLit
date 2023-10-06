// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include "Engine/EngineTypes.h"
#include "Tickable.h"

class ADisplayClusterRootActor;

/**
 * Preview manager receive events from DCRAs
 */
enum class EDisplayClusterRootActorPreviewEvent : uint8
{
	// DCRA created
	Create = 0,

	// DCRA removed
	Remove,

	// DCRA wants to render preview
	Render
};

/**
 * Container for DCRA objects that are used for preview rendering.
 */
class FDisplayClusterRootActorPreviewObject
{
public:
	FDisplayClusterRootActorPreviewObject() = default;
	FDisplayClusterRootActorPreviewObject(ADisplayClusterRootActor* InRootActor)
		: RootActorWeakPtr(InRootActor)
	{ }

public:
	/** Returns true if the DCRA object still exists. */
	inline bool IsValid() const
	{
		return RootActorWeakPtr.IsValid() && !RootActorWeakPtr.IsStale();
	}

	/** Returns true if the DCRA preview can be performed right now. */
	inline bool ShouldRenderPreview() const
	{
		return bRenderPreviewRequested && !bPreviewRenderingCompleted;
	}

	/** Changes the internal flags of this DCRA container to a state indicating that the preview rendering is complete. */
	inline void MarkAsRendered()
	{
		bRenderPreviewRequested = false;
		bPreviewRenderingCompleted = true;
	}

	/** Changes the internal flags of this DCRA container to a state that allows render previews. */
	inline void MarkAsNotRendered()
	{
		bPreviewRenderingCompleted = false;
	}

	/** Returns true if the InRootActor is the same as the one used inside this container. */
	inline bool IsEqual(const ADisplayClusterRootActor* InRootActor) const
	{
		return GetRootActor() == InRootActor;
	}

	/** Return the pointer to the DCRA actor if it still exists. */
	inline ADisplayClusterRootActor* GetRootActor() const
	{
		return RootActorWeakPtr.IsValid() ? RootActorWeakPtr.Get() : nullptr;
	}

	/** When DCRA tries to call the preview rendering, this function will be called. */
	void HandleRenderRequest();

	/** Performs a preview of the rendering for DCRA in this container. */
	void RenderPreview();

private:
	// Weak pointer to the DCRA
	TWeakObjectPtr<ADisplayClusterRootActor> RootActorWeakPtr;

	// Preview is already rendered for this actor
	bool bPreviewRenderingCompleted = false;

	// Preview render request from DCRA
	bool bRenderPreviewRequested = false;
};

/**
 * This is a singleton class that controls the preview rendering of multiple DCRA objects.
 */
class FDisplayClusterRootActorPreviewRenderingManager
{
public:
	/** The entry point for DCRA, which is used to handle all workflow events related to preview rendering. */
	static void HandleEvent(const EDisplayClusterRootActorPreviewEvent InDCRAEvent, ADisplayClusterRootActor* InRootActor);

protected:
	friend class FDisplayClusterPreviewTickableGameObject;

	/** This function is used to perform delayed DCRA preview rendering requests. */
	void Tick(float DeltaTime);

protected:
	/** Implementation of the static HandleEvent() function. */
	void HandleEventImpl(const EDisplayClusterRootActorPreviewEvent InDCRAEvent, ADisplayClusterRootActor* InRootActor);

	/** Creating or deleting a Tickable object depends on whether the DCRAs list is not empty. */
	void UpdateTickableGameObject();

	/** Gets the index of the next DCRA container, which can be used for rendering the preview. */
	int32  GetNextPreviewObject();

	/** When all registered DCRA objects are drawn, you must call this function to reset the internal flags. */
	void BeginNewRenderCycle();

private:
	// List of existing DCRAs
	TArray<FDisplayClusterRootActorPreviewObject> PreviewObjects;

	// Tickable object to call preview rendering for single DCRA per frame
	TUniquePtr<FTickableGameObject> TickableGameObject;
};
