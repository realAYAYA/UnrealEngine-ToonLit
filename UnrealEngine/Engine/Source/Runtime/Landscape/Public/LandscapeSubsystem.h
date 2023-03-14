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

#if WITH_EDITOR
	LANDSCAPE_API void BuildAll();
	LANDSCAPE_API void BuildGrassMaps();
	LANDSCAPE_API int32 GetOutdatedGrassMapCount();
	LANDSCAPE_API void BuildGIBakedTextures();
	LANDSCAPE_API int32 GetOutdatedGIBakedTextureComponentsCount();
	LANDSCAPE_API void BuildPhysicalMaterial();
	LANDSCAPE_API int32 GetOudatedPhysicalMaterialComponentsCount();
	LANDSCAPE_API void BuildNanite();
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
#endif // WITH_EDITOR

private:
#if WITH_EDITOR
	LANDSCAPE_API void ForEachLandscapeInfo(TFunctionRef<bool(ULandscapeInfo*)> ForEachLandscapeInfoFunc) const;
#endif

	// Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// End USubsystem

	TArray<TWeakObjectPtr<ALandscapeProxy>> Proxies;

#if WITH_EDITOR
	class FLandscapeGrassMapsBuilder* GrassMapsBuilder;
	class FLandscapeGIBakedTextureBuilder* GIBakedTextureBuilder;
	class FLandscapePhysicalMaterialBuilder* PhysicalMaterialBuilder;
	
	FLandscapeNotificationManager* NotificationManager;
	FOnHeightmapStreamedDelegate OnHeightmapStreamed;
#endif // WITH_EDITOR
};
