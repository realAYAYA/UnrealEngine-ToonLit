// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Subsystems/WorldSubsystem.h"
#include "LandscapeSubsystem.generated.h"

class ALandscapeProxy;
class ALandscape;
class AWorldSettings;
class IConsoleVariable;
class ULandscapeInfo;
class FLandscapeNotificationManager;
class ULandscapeComponent;
class FLandscapeGrassMapsBuilder;
class FLandscapeTextureStreamingManager;
struct FActionableMessage;
struct FDateTime;
struct FScopedSlowTask;

namespace UE::Landscape
{
	enum class EOutdatedDataFlags : uint8;

#if WITH_EDITOR
	void LANDSCAPE_API MarkModifiedLandscapesAsDirty();
	void LANDSCAPE_API BuildGrassMaps();
	void LANDSCAPE_API BuildPhysicalMaterial();
	void LANDSCAPE_API BuildNanite();
	void LANDSCAPE_API BuildAll();
#endif // WITH_EDITOR
} // end of namespace UE::Landscape

#if WITH_EDITOR
struct FOnHeightmapStreamedContext
{
private:
	const FBox2D& UpdateRegion;
	const TSet<class ULandscapeComponent*>& LandscapeComponentsInvolved;

public:
	const FBox2D& GetUpdateRegion() const { return UpdateRegion; }
	const TSet<class ULandscapeComponent*>& GetLandscapeComponentsInvolved() const { return LandscapeComponentsInvolved; }

	FOnHeightmapStreamedContext(const FBox2D& updateRegion, const TSet<class ULandscapeComponent*>& landscapeComponentsInvolved)
		: UpdateRegion(updateRegion), LandscapeComponentsInvolved(landscapeComponentsInvolved)
	{}
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHeightmapStreamedDelegate, const FOnHeightmapStreamedContext& context);
#endif // WITH_EDITOR

UCLASS(MinimalAPI)
class ULandscapeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULandscapeSubsystem();
	virtual ~ULandscapeSubsystem();

	void RegisterActor(ALandscapeProxy* Proxy);
	void UnregisterActor(ALandscapeProxy* Proxy);

	// Begin FTickableGameObject overrides
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual TStatId GetStatId() const override;
	// End FTickableGameObject overrides

	// setting this to true causes grass instance generation to go wider (multiplies the limits by GGrassCreationPrioritizedMultipler)
	void PrioritizeGrassCreation(bool bPrioritizeGrassCreation) { bIsGrassCreationPrioritized = bPrioritizeGrassCreation; }
	bool IsGrassCreationPrioritized() const { return bIsGrassCreationPrioritized; }
	FLandscapeGrassMapsBuilder* GetGrassMapBuilder() { return GrassMapsBuilder; }
	FLandscapeTextureStreamingManager* GetTextureStreamingManager() { return TextureStreamingManager; }

	/**
	 * Can be called at runtime : (optionally) flushes grass on all landscape components and updates them
	 * @param bInFlushGrass : flushes all grass from landscape components prior to updating them
	 * @param bInForceSync : synchronously updates grass on all landscape components 
	 * @param InOptionalCameraLocations : (optional) camera locations that should be used when updating the grass. If not specified, the usual (streaming manager-based) view locations will be used
	 */
	LANDSCAPE_API void RegenerateGrass(bool bInFlushGrass, bool bInForceSync, TOptional<TArrayView<FVector>> InOptionalCameraLocations = TOptional<TArrayView<FVector>>());
	
	// Remove all grass instances from the specified components.  If passed null, removes all grass instances from all proxies.
	void RemoveGrassInstances(const TSet<ULandscapeComponent*>* ComponentsToRemoveGrassInstances = nullptr);

	// called when components are registered to the world	
	void RegisterComponent(ULandscapeComponent* Component);
	void UnregisterComponent(ULandscapeComponent* Component);

#if WITH_EDITOR
	LANDSCAPE_API void BuildAll();

	// Synchronously build grass maps for all components
	LANDSCAPE_API void BuildGrassMaps();
	LANDSCAPE_API void BuildPhysicalMaterial();

	UE_DEPRECATED(5.3, "BuildGIBakedTextures is officially deprecated nowe")
	void BuildGIBakedTextures() {}
	UE_DEPRECATED(5.3, "GetOutdatedGIBakedTextureComponentsCount is officially deprecated now")
	int32 GetOutdatedGIBakedTextureComponentsCount() { return 0; }

	/**
	 * Updates the Nanite mesh on all landscape actors whose mesh is not up to date.
	 * @param InProxiesToBuild - If specified, only the Nanite meshes of the specified landscape actors (recursively for all streaming proxies, in the case of a 1 ALandscape / N ALandscapeStreamingProxy setup) will be built
	 * @param bForceRebuild - If true, forces the Nanite meshes to be rebuilt, no matter if they're up to date or not
	 */
	LANDSCAPE_API void BuildNanite(TArrayView<ALandscapeProxy*> InProxiesToBuild = TArrayView<ALandscapeProxy*>(), bool bForceRebuild = false);
	
	LANDSCAPE_API TArray<ALandscapeProxy*> GetOutdatedProxies(UE::Landscape::EOutdatedDataFlags InMatchingOutdatedDataFlags, bool bInMustMatchAllFlags) const;
	
	UE_DEPRECATED(5.3, "GetOutdatedGrassMapCount is now deprecated, use GetOutdatedProxies")
	LANDSCAPE_API int32 GetOutdatedGrassMapCount();
	UE_DEPRECATED(5.3, "GetOudatedPhysicalMaterialComponentsCount is now deprecated, use GetOutdatedProxies")
	LANDSCAPE_API int32 GetOudatedPhysicalMaterialComponentsCount();

	LANDSCAPE_API bool IsGridBased() const;
	LANDSCAPE_API void ChangeGridSize(ULandscapeInfo* LandscapeInfo, uint32 NewGridSizeInComponents);
	LANDSCAPE_API ALandscapeProxy* FindOrAddLandscapeProxy(ULandscapeInfo* LandscapeInfo, const FIntPoint& SectionBase);

	UE_DEPRECATED(5.5, "DisplayMessages is now deprecated.")
	LANDSCAPE_API void DisplayMessages(class FCanvas* Canvas, float& XPos, float& YPos);

	LANDSCAPE_API bool GetActionableMessage(FActionableMessage& OutActionableMessage);
	LANDSCAPE_API void MarkModifiedLandscapesAsDirty();
	LANDSCAPE_API void SaveModifiedLandscapes();
	LANDSCAPE_API bool HasModifiedLandscapes() const;
	UE_DEPRECATED(5.3, "Use GetDirtyOnlyInMode instead")
	LANDSCAPE_API static bool IsDirtyOnlyInModeEnabled();
	LANDSCAPE_API bool GetDirtyOnlyInMode() const;
	FLandscapeNotificationManager* GetNotificationManager() { return NotificationManager; }
	FOnHeightmapStreamedDelegate& GetOnHeightmapStreamedDelegate() { return OnHeightmapStreamed; }
	bool AnyViewShowCollisions() const { return bAnyViewShowCollisions; }  //! Returns true if any view has view collisions enabled.
	FDateTime GetAppCurrentDateTime();
	LANDSCAPE_API void AddAsyncEvent(FGraphEventRef GraphEventRef);

	// Returns true if we should build nanite meshes in parallel asynchronously. 
	bool IsMultithreadedNaniteBuildEnabled();

	// Returns true if the user has requested Nanite Meshes to be generated on landscape edit. If we return false then the nanite build will happen either on map save or explicit build 
	bool IsLiveNaniteRebuildEnabled();

	bool AreNaniteBuildsInProgress() const;
	void IncNaniteBuild();
	void DecNaniteBuild();

	// Wait unit we're able to continue a landscape export task (Max concurrent nanite mesh builds is defined by  landscape.Nanite.MaxSimultaneousMultithreadBuilds and landscape.Nanite.MultithreadBuild CVars)
	void WaitLaunchNaniteBuild(); 
#endif // WITH_EDITOR

private:
	LANDSCAPE_API void ForEachLandscapeInfo(TFunctionRef<bool(ULandscapeInfo*)> ForEachLandscapeInfoFunc) const;

	// Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// End USubsystem

	void OnNaniteWorldSettingsChanged(AWorldSettings* WorldSettings) { RegenerateGrass(true, true); }
	void OnNaniteEnabledChanged(IConsoleVariable*);

	void HandlePostGarbageCollect();

	bool bIsGrassCreationPrioritized = false;
	TArray<TWeakObjectPtr<ALandscape>> LandscapeActors;
	TArray<TWeakObjectPtr<ALandscapeProxy>> Proxies;
	FDelegateHandle OnNaniteWorldSettingsChangedHandle;

	FLandscapeTextureStreamingManager* TextureStreamingManager = nullptr;
	FLandscapeGrassMapsBuilder* GrassMapsBuilder = nullptr;

#if WITH_EDITOR
	class FLandscapePhysicalMaterialBuilder* PhysicalMaterialBuilder = nullptr;
	
	FLandscapeNotificationManager* NotificationManager = nullptr;
	FOnHeightmapStreamedDelegate OnHeightmapStreamed;
	bool bAnyViewShowCollisions = false;
	FDateTime AppCurrentDateTime; // Represents FDateTime::Now(), at the beginning of the frame (useful to get a human-readable date/time that is fixed during the frame)
	int32 LastTickFrameNumber = -1;

	TArray<FGraphEventRef> NaniteMeshBuildEvents;
	float NumNaniteMeshUpdatesAvailable = 0.0f;

	std::atomic<int32> NaniteBuildsInFlight;
	std::atomic<int32> NaniteStaticMeshesInFlight;

#endif // WITH_EDITOR
	
	FDelegateHandle OnScalabilityChangedHandle;
};
