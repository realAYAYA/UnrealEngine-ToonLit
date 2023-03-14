// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WaterBodyActor.h"
#include "UObject/ObjectMacros.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/EngineTypes.h"
#include "Interfaces/Interface_PostProcessVolume.h"
#include "WaterBodyManager.h"
#include "WaterZoneActor.h"
#include "WaterSubsystem.generated.h"

DECLARE_STATS_GROUP(TEXT("Water"), STATGROUP_Water, STATCAT_Advanced);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCameraUnderwaterStateChanged, bool, bIsUnderWater, float, DepthUnderwater);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWaterScalabilityChanged);

class UWaterBodyComponent;
class UMaterialParameterCollection;
class UWaterRuntimeSettings;
class FSceneView;
class UTexture2D;
struct FUnderwaterPostProcessDebugInfo;
enum class EWaterBodyQueryFlags;
class ABuoyancyManager;
class FWaterViewExtension;

namespace UE::WaterInfo { struct FRenderingContext; }

bool IsWaterEnabled(bool bIsRenderThread);

struct FUnderwaterPostProcessVolume : public IInterface_PostProcessVolume
{
	FUnderwaterPostProcessVolume()
		: PostProcessProperties()
	{}

	virtual bool EncompassesPoint(FVector Point, float SphereRadius/*=0.f*/, float* OutDistanceToPoint) override
	{
		// For underwater, the distance to point is 0 for now because underwater doesn't look correct if it is blended with other post process due to the wave masking
		if (OutDistanceToPoint)
		{
			*OutDistanceToPoint = 0;
		}

		// If post process properties are enabled and valid return true.  We already computed if it encompasses the water volume earlier
		return PostProcessProperties.bIsEnabled && PostProcessProperties.Settings;
	}

	virtual FPostProcessVolumeProperties GetProperties() const override
	{
		return PostProcessProperties;
	}

#if DEBUG_POST_PROCESS_VOLUME_ENABLE
	virtual FString GetDebugName() const override
	{
		return FString("UnderwaterPostProcessVolume");
	}
#endif

	FPostProcessVolumeProperties PostProcessProperties;
};

/**
 * This is the API used to get information about water at runtime
 */
UCLASS(BlueprintType, Transient)
class WATER_API UWaterSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UWaterSubsystem();

	// FTickableGameObject implementation Begin
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// FTickableGameObject implementation End

	// UWorldSubsystem implementation Begin
	/** Override to support water subsystems in editor preview worlds */
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	// UWorldSubsystem implementation End

	// USubsystem implementation Begin
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void PostInitialize() override;
	virtual void Deinitialize() override;
	// USubsystem implementation End

	/** Static helper function to get a water subsystem from a world, returns nullptr if world or subsystem don't exist */
	static UWaterSubsystem* GetWaterSubsystem(const UWorld* InWorld);

	/** Static helper function to get a waterbody manager from a world, returns nullptr if world or manager don't exist */
	static FWaterBodyManager* GetWaterBodyManager(const UWorld* InWorld);

	/** Static helper function to get a weak ptr to the water scene view extension for a given world. */
	static TWeakPtr<FWaterViewExtension, ESPMode::ThreadSafe> GetWaterViewExtension(const UWorld* InWorld);

	UE_DEPRECATED(5.1, "Please use FWaterBodyManager::ForEachWaterBodyComponent instead.")
	static void ForEachWaterBodyComponent(const UWorld* World, TFunctionRef<bool(UWaterBodyComponent*)> Predicate) {}

	UE_DEPRECATED(5.1, "This will become a private member. Prefer calling GetWaterBodyManager instead")
	FWaterBodyManager WaterBodyManager;

	UE_DEPRECATED(5.1, "There may be multiple water zones per level. Prefer calling the GetWaterZone on a per-water body basis or iterating over all zones.")
	AWaterZone* GetWaterZoneActor(ULevel* InPreferredOuterLevel = nullptr) const { return nullptr; }

	ABuoyancyManager* GetBuoyancyManager() const { return BuoyancyManager; }

	TWeakObjectPtr<UWaterBodyComponent> GetOceanBodyComponent() { return OceanBodyComponent; }
	void SetOceanBodyComponent(TWeakObjectPtr<UWaterBodyComponent> InOceanBodyComponent) { OceanBodyComponent = InOceanBodyComponent; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category=Water)
	bool IsShallowWaterSimulationEnabled() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Water)
	bool IsUnderwaterPostProcessEnabled() const;

	UFUNCTION(BlueprintCallable, Category=Water)
	static int32 GetShallowWaterMaxDynamicForces();

	UFUNCTION(BlueprintCallable, Category = Water)
	static int32 GetShallowWaterMaxImpulseForces();

	UFUNCTION(BlueprintCallable, Category = Water)
	static int32 GetShallowWaterSimulationRenderTargetSize();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Water)
	bool IsWaterRenderingEnabled() const;

	UFUNCTION(BlueprintCallable, Category = Water)
	float GetWaterTimeSeconds() const;

	UFUNCTION(BlueprintCallable, Category = Water)
	float GetSmoothedWorldTimeSeconds() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Water)
	float GetCameraUnderwaterDepth() const { return CachedDepthUnderwater; }

	UFUNCTION(BlueprintCallable, Category = Water)
	void PrintToWaterLog(const FString& Message, bool bWarning);

	/** Returns the base height of the ocean. This should correspond to its world Z position */
	UFUNCTION(BlueprintCallable, Category = Water)
	float GetOceanBaseHeight() const;

	/** Returns the relative flood height */
	UFUNCTION(BlueprintCallable, Category = Water)
	float GetOceanFloodHeight() const { return FloodHeight; }

	/** Returns the total height of the ocean. This should correspond to the base height plus any additional height, like flood for example */
	UFUNCTION(BlueprintCallable, Category = Water)
	float GetOceanTotalHeight() const { return GetOceanBaseHeight() + GetOceanFloodHeight(); }

	UFUNCTION(BlueprintCallable, Category = Water)
	void SetOceanFloodHeight(float InFloodHeight);

	void SetSmoothedWorldTimeSeconds(float InTime);
	
	void SetOverrideSmoothedWorldTimeSeconds(float InTime);
	float GetOverrideSmoothedWorldTimeSeconds() const { return OverrideWorldTimeSeconds; }
	
	void SetShouldOverrideSmoothedWorldTimeSeconds(bool bOverride);
	bool GetShouldOverrideSmoothedWorldTimeSeconds() const { return bUsingOverrideWorldTimeSeconds; }

	void SetShouldPauseWaveTime(bool bInPauseWaveTime);

	UMaterialParameterCollection* GetMaterialParameterCollection() const {	return MaterialParameterCollection; }
	
	void MarkAllWaterZonesForRebuild(EWaterZoneRebuildFlags RebuildFlags = EWaterZoneRebuildFlags::All);
	void MarkWaterZonesInRegionForRebuild(const FBox2D& InUpdateRegion, EWaterZoneRebuildFlags InRebuildFlags);

#if WITH_EDITOR
	/** Little scope object to temporarily change the value of bAllowWaterSubsystemOnPreviewWorld */
	struct WATER_API FScopedAllowWaterSubsystemOnPreviewWorld
	{
		FScopedAllowWaterSubsystemOnPreviewWorld(bool bNewValue);
		~FScopedAllowWaterSubsystemOnPreviewWorld();

		// Non-copyable
	private:
		FScopedAllowWaterSubsystemOnPreviewWorld() = delete;
		FScopedAllowWaterSubsystemOnPreviewWorld& operator=(const FScopedAllowWaterSubsystemOnPreviewWorld&) = delete;
		FScopedAllowWaterSubsystemOnPreviewWorld(const FScopedAllowWaterSubsystemOnPreviewWorld&) = delete;

		bool bPreviousValue = false;
	};
	static void SetAllowWaterSubsystemOnPreviewWorld(bool bInValue) { bAllowWaterSubsystemOnPreviewWorld = bInValue; }
	static bool GetAllowWaterSubsystemOnPreviewWorld() { return bAllowWaterSubsystemOnPreviewWorld; }
#endif // WITH_EDITOR

private:
	void NotifyWaterScalabilityChangedInternal(IConsoleVariable* CVar);
	void NotifyWaterVisibilityChangedInternal(IConsoleVariable* CVar);
	void ComputeUnderwaterPostProcess(FVector ViewLocation, FSceneView* SceneView);
	void SetMPCTime(float Time, float PrevTime);
	void AdjustUnderwaterWaterInfoQueryFlags(EWaterBodyQueryFlags& InOutFlags);
	void ApplyRuntimeSettings(const UWaterRuntimeSettings* Settings, EPropertyChangeType::Type ChangeType);

	FWaterBodyManager& GetWaterBodyManagerInternal();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	void ShowOnScreenDebugInfo(const FVector& InViewLocation, const FUnderwaterPostProcessDebugInfo& InDebugInfo);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

public:
	UPROPERTY(Transient)
	TObjectPtr<ABuoyancyManager> BuoyancyManager;

	DECLARE_EVENT_OneParam(UWaterSubsystem, FOnWaterSubsystemInitialized, UWaterSubsystem*)
	static FOnWaterSubsystemInitialized OnWaterSubsystemInitialized;

	UPROPERTY(BlueprintAssignable, Category = Water)
	FOnCameraUnderwaterStateChanged OnCameraUnderwaterStateChanged;

	UPROPERTY(BlueprintAssignable, Category = Water)
	FOnWaterScalabilityChanged OnWaterScalabilityChanged;

	UPROPERTY()
	TObjectPtr<UStaticMesh> DefaultRiverMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> DefaultLakeMesh;

private:

	TWeakObjectPtr<UWaterBodyComponent> OceanBodyComponent;

	ECollisionChannel UnderwaterTraceChannel;

	float CachedDepthUnderwater;
	float SmoothedWorldTimeSeconds;
	float NonSmoothedWorldTimeSeconds;
	float PrevWorldTimeSeconds;
	float OverrideWorldTimeSeconds;
	float FloodHeight = 0.0f;
	bool bUsingSmoothedTime;
	bool bUsingOverrideWorldTimeSeconds;
	bool bUnderWaterForAudio;
	bool bPauseWaveTime;

	/** The parameter collection asset that holds the global parameters that are updated by this actor */
	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> MaterialParameterCollection;

	FUnderwaterPostProcessVolume UnderwaterPostProcessVolume;

	TSharedPtr<FWaterViewExtension> WaterViewExtension;

#if WITH_EDITOR
	FDelegateHandle OnHeightmapStreamedHandle;
	void OnHeightmapStreamed(const struct FOnHeightmapStreamedContext& InContext);

	/** By default, there is no water subsystem allowed on preview worlds except when explicitly requested : */
	static bool bAllowWaterSubsystemOnPreviewWorld;
#endif // WITH_EDITOR
};
