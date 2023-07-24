// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Subsystems/WorldSubsystem.h"
#include "LandscapeSubsystem.generated.h"

class ALandscapeProxy;
class ULandscapeInfo;
class FLandscapeNotificationManager;

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

	void PrioritizeGrassCreation(bool bPrioritizeGrassCreation) { bIsGrassCreationPrioritized = bPrioritizeGrassCreation; }
	bool IsGrassCreationPrioritized() const { return bIsGrassCreationPrioritized; }

	/**
	 * Can be called at runtime : (optionally) flushes grass on all landscape components and updates them
	 * @param bInFlushGrass : flushes all grass from landscape components prior to updating them
	 * @param bInForceSync : synchronously updates grass on all landscape components 
	 * @param InOptionalCameraLocations : (optional) camera locations that should be used when updating the grass. If not specified, the usual (streaming manager-based) view locations will be used
	 */
	LANDSCAPE_API void RegenerateGrass(bool bInFlushGrass, bool bInForceSync, TOptional<TArrayView<FVector>> InOptionalCameraLocations = TOptional<TArrayView<FVector>>());

#if WITH_EDITOR
	LANDSCAPE_API void BuildAll();
	LANDSCAPE_API void BuildGrassMaps();
	LANDSCAPE_API int32 GetOutdatedGrassMapCount();
	LANDSCAPE_API void BuildGIBakedTextures();
	LANDSCAPE_API int32 GetOutdatedGIBakedTextureComponentsCount();
	LANDSCAPE_API void BuildPhysicalMaterial();
	LANDSCAPE_API int32 GetOudatedPhysicalMaterialComponentsCount();
	/**
	 * Updates the Nanite mesh on all landscape actors whose mesh is not up to date.
	 * @param InProxiesToBuild - If specified, only the Nanite meshes of the specified landscape actors (recursively for all streaming proxies, in the case of a 1 ALandscape / N ALandscapeStreamingProxy setup) will be built
	 * @param bForceRebuild - If true, forces the Nanite meshes to be rebuilt, no matter if they're up to date or not
	 */
	LANDSCAPE_API void BuildNanite(TArrayView<ALandscapeProxy*> InProxiesToBuild = TArrayView<ALandscapeProxy*>(), bool bForceRebuild = false);
	LANDSCAPE_API bool IsGridBased() const;
	LANDSCAPE_API void ChangeGridSize(ULandscapeInfo* LandscapeInfo, uint32 NewGridSizeInComponents);
	LANDSCAPE_API ALandscapeProxy* FindOrAddLandscapeProxy(ULandscapeInfo* LandscapeInfo, const FIntPoint& SectionBase);
	LANDSCAPE_API void DisplayMessages(class FCanvas* Canvas, float& XPos, float& YPos);
	LANDSCAPE_API void MarkModifiedLandscapesAsDirty();
	LANDSCAPE_API void SaveModifiedLandscapes();
	LANDSCAPE_API bool HasModifiedLandscapes() const;
	LANDSCAPE_API static bool IsDirtyOnlyInModeEnabled();
	FLandscapeNotificationManager* GetNotificationManager() { return NotificationManager; }
	FOnHeightmapStreamedDelegate& GetOnHeightmapStreamedDelegate() { return OnHeightmapStreamed; }
	LANDSCAPE_API bool AnyViewShowCollisions() const { return bAnyViewShowCollisions; }  //! Returns true if any view has view collisions enabled.
#endif // WITH_EDITOR

private:
#if WITH_EDITOR
	LANDSCAPE_API void ForEachLandscapeInfo(TFunctionRef<bool(ULandscapeInfo*)> ForEachLandscapeInfoFunc) const;
#endif

	// Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// End USubsystem

	bool bIsGrassCreationPrioritized;
	TArray<TWeakObjectPtr<ALandscapeProxy>> Proxies;

#if WITH_EDITOR
	class FLandscapeGrassMapsBuilder* GrassMapsBuilder;
	class FLandscapeGIBakedTextureBuilder* GIBakedTextureBuilder;
	class FLandscapePhysicalMaterialBuilder* PhysicalMaterialBuilder;
	
	FLandscapeNotificationManager* NotificationManager;
	FOnHeightmapStreamedDelegate OnHeightmapStreamed;
	bool bAnyViewShowCollisions = false;
#endif // WITH_EDITOR
};
