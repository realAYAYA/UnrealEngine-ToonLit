// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ContentStreaming.h"

class UStreamableRenderAsset;
class ULevel;
namespace Nanite { using CoarseMeshStreamingHandle = int16; }

namespace Nanite
{
class FCoarseMeshStreamingManager : public IStreamingManager
{
public:

	FCoarseMeshStreamingManager();

	// IStreamingManager interface
	virtual void UpdateResourceStreaming(float DeltaTime, bool bProcessEverything = false) override;
	virtual int32 BlockTillAllRequestsFinished(float TimeLimit = 0.0f, bool bLogResults = false) override;
	virtual void CancelForcedResources() override;

	virtual void NotifyActorDestroyed(AActor* Actor) override;
	virtual void NotifyPrimitiveDetached(const UPrimitiveComponent* Primitive) override;
	virtual void NotifyPrimitiveUpdated(const UPrimitiveComponent* Primitive) override;
	virtual void NotifyPrimitiveUpdated_Concurrent(const UPrimitiveComponent* Primitive) override;

	virtual void AddLevel(class ULevel* Level) override;
	virtual void RemoveLevel(class ULevel* Level) override;

	virtual void NotifyLevelChange() override {}
	virtual void SetDisregardWorldResourcesForFrames(int32 NumFrames) override {}
	virtual void NotifyLevelOffset(class ULevel* Level, const FVector& Offset) override {}
	// End IStreamingManager interface

	/** Register/Unregister streamable nanite coarse mesh static assets */
	void RegisterRenderAsset(UStreamableRenderAsset* RenderAsset);
	void UnregisterRenderAsset(UStreamableRenderAsset* RenderAsset);

	/** Request rebuild of the cached render state for the given handle */
	void RequestUpdateCachedRenderState(const UStreamableRenderAsset* RenderAsset);

	/** Add the used streaming handles used by a single FSceneRenderer - called on RenderThread */
	ENGINE_API void AddUsedStreamingHandles(TArray<CoarseMeshStreamingHandle>& UsedHandles);

	/** Process all the used streaming handles used by all the scene renderers and find out the new state of the registered resources - called on RenderThread */
	ENGINE_API void UpdateResourceStates();

#if WITH_EDITOR
	/** In the editor force a refresh of all cached commands when the coarse LOD mode changes */
	ENGINE_API static bool CheckStreamingMode();
	static int32 GetStreamingMode() { return CachedNaniteCoarseMeshStreamingMode; }
#endif // WITH_EDITOR

private:
		
	enum class ENotifyMode
	{
		Register,
		Unregister,
	};
	void ProcessNotifiedActor(const AActor* Actor, ENotifyMode NotifyMode);

	/** Register/Unregister a primitive component */
	void RegisterComponent(const UPrimitiveComponent* Primitive, bool bCheckStaticMeshChanged);
	void UnregisterComponent(const UPrimitiveComponent* Primitive);
	void UnregisterComponentInternal(const UPrimitiveComponent* Primitive, UStreamableRenderAsset* RenderAsset);

	/** Helper function for forces stream in/out of all assets */
	void ForceStreamIn();
	void ForceStreamOut();

	// General update lock
	FCriticalSection UpdateCS;

	// Render assets which don't have valid resources anymore
	TArray<CoarseMeshStreamingHandle> ReleasedRenderAssets;

	// Used streamable handles collected from all the last rendering update rounds
	TSet<CoarseMeshStreamingHandle> UsedStreamableHandles;
	TArray<CoarseMeshStreamingHandle> TmpUsedStreamableHandles;

	enum class EResourceState
	{
		Unloaded,
		RequestStreamIn,
		StreamingIn,
		Loaded,
		RequestStreamOut,
		StreamingOut,
	};

	// Registered streamable render assets
	struct FRegisteredRenderAsset
	{
		// Unique handle for the regisered assets - used as index into RegisteredRenderAssets for fast retrieval
		CoarseMeshStreamingHandle Handle = INDEX_NONE;

		// Actual streamable render asset on which the stream in/out operations will be called
		UStreamableRenderAsset* RenderAsset = nullptr;

		// All the components using the render asset and need to be informed on stream in/out requests
		//TArray<const UPrimitiveComponent*> PrimitiveComponents;

		// Cache streaming size
		uint32 StreamingSize = 0;

		// Last used tick index - used for stream out order
		uint64 LastUsedTickIndex = 0;

		// Current state of the resource
		EResourceState ResourceState = EResourceState::Unloaded;
	};
	TSparseArray<FRegisteredRenderAsset> RegisteredRenderAssets;
	TArray<CoarseMeshStreamingHandle> ActiveHandles;
	TArray<CoarseMeshStreamingHandle> TmpHandleArray;

	TArray<ULevel*> RegisteredLevels;
	TMap<UStreamableRenderAsset*, TSet<const UPrimitiveComponent*>> RequestReleaseComponentsMap;
	TMap<UStreamableRenderAsset*, TSet<const UPrimitiveComponent*>> RegisteredComponentsMap;
	TMap<const UPrimitiveComponent*, UStreamableRenderAsset*> ComponentToRenderAssetLookUpMap;

	uint64 TotalRequestedRenderAssetSize = 0;
	uint64 TotalLoadedRenderAssetSize = 0;
	uint64 TickCount = 0;

#if WITH_EDITOR
	static int32 CachedNaniteCoarseMeshStreamingMode;
#endif // WITH_EDITOR
};

} // namespace Nanite

